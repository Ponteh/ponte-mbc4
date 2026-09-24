from pathlib import Path
import shutil,subprocess
ROOT=Path(__file__).resolve().parents[3]
BUILD=ROOT/'build/mc2000-nap-2026-09-23/paired'
BASE=BUILD/'baseline-model9-Source'
BUILD.mkdir(parents=True,exist_ok=True)
if not BASE.exists():
 shutil.copytree(ROOT/'build/mc2000-nap-2026-09-23/baseline-Source',BASE)
 p=BASE/'DSP/Ballistics.h';p.write_text(p.read_text().replace('sampleRate * 0.00002','sampleRate * 0.00032'))
 p=BASE/'DSP/MultiBandCompressor.h';p.write_text(p.read_text().replace('dspModelVersion = 8','dspModelVersion = 9'))
source=ROOT/'products/MC2000/Source';research=source.parent/'Research'
lines=['cmake_minimum_required(VERSION 3.24)','project(PerformancePair LANGUAGES CXX)','set(CMAKE_CXX_STANDARD 20)']
for name,src in [('baseline',BASE),('current',source)]:
 lines += [f'add_library({name} OBJECT "{research.as_posix()}/PerformancePairEngine.cpp" "{src.as_posix()}/DSP/MultiBandCompressor.cpp" "{src.as_posix()}/DSP/CrossoverNetwork.cpp")',f'target_include_directories({name} PRIVATE "{src.as_posix()}")',f'target_compile_definitions({name} PRIVATE BENCH_PREFIX={name} pontedsp={name}_pontedsp)']
lines += [f'add_executable(PerformancePair "{research.as_posix()}/PerformancePair.cpp" $<TARGET_OBJECTS:baseline> $<TARGET_OBJECTS:current>)']
(BUILD/'CMakeLists.txt').write_text('\n'.join(lines)+'\n')
cmake='C:/cmake-3.30.1-windows-x86_64/bin/cmake.exe'
subprocess.run([cmake,'-S',str(BUILD),'-B',str(BUILD/'out'),'-G','Visual Studio 17 2022','-A','x64'],check=True)
subprocess.run([cmake,'--build',str(BUILD/'out'),'--config','Release','-j','2'],check=True)
