param([string]$OutputDirectory = (Join-Path $PSScriptRoot 'audio'))
$ErrorActionPreference = 'Stop'
Add-Type -Path (Join-Path $PSScriptRoot 'MeterStimuli.cs')
$destination = [IO.Path]::GetFullPath($OutputDirectory)
[IO.Directory]::CreateDirectory($destination) | Out-Null
$reports = [PonteMeterStimuli]::Generate($destination)
$manifest = [ordered]@{
    packVersion = 1
    sampleRate = 48000
    channels = 2
    channelLayout = 'dual mono, L equals R; levels refer to each channel'
    format = 'WAV PCM signed 24-bit little endian, no dither'
    transitionSamples = 96
    transition = '2 ms half-cosine amplitude ramp at each segment start; carrier phase continuous at unchanged frequency'
    silenceDbMarker = -120
    notes = 'Events mark ramp START, not its end. -120 denotes digital silence, except the first 96 samples of its transition. Harmonic probe is synthetic, not a real voice.'
    files = @($reports)
}
$utf8 = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $PSScriptRoot 'manifest.json'), ($manifest | ConvertTo-Json -Depth 8), $utf8)
$events = foreach ($file in $reports) {
    foreach ($event in $file.Events) {
        [pscustomobject]@{ file=$file.File; event=$event.Label; startSample=$event.StartSample;
            endSample=$event.EndSample; startSeconds=$event.StartSample/48000.0;
            endSeconds=$event.EndSample/48000.0; frequencyHz=$event.Frequency;
            targetPeakDbfs=$event.LevelDb; signal=$event.Signal }
    }
}
$events | Export-Csv -NoTypeInformation -Encoding UTF8 (Join-Path $PSScriptRoot 'events.csv')
$reports | Select-Object File,DurationSeconds,PeakDbfs,Bytes,Sha256 | Format-Table -AutoSize
