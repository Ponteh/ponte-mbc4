"""Identify Auto attack/BITE from original isolated carriers; no production writes."""
from pathlib import Path
import argparse, ctypes, json, subprocess, sys
import numpy as np
from scipy.optimize import minimize_scalar
sys.path.insert(0,str(Path(__file__).resolve().parent))
from analyse_2026_09_23 import ROOT, OUT, BUILD, save
from analyse import audio

def main(build=False):
    if build:
        folder=BUILD/'fit';folder.mkdir(exist_ok=True,parents=True)
        (folder/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.22)\nproject(AutoBiteFit LANGUAGES CXX)\n'
            +'add_library(AutoBiteFit SHARED "'+(ROOT.parent/'AutoBiteFit.cpp').as_posix()+'")\n')
        subprocess.run(['cmake','-S',str(folder),'-B',str(folder/'build'),'-G','Visual Studio 17 2022','-A','x64'],check=True)
        subprocess.run(['cmake','--build',str(folder/'build'),'--config','Release'],check=True)
    dll=ctypes.CDLL(str(BUILD/'fit/build/Release/AutoBiteFit.dll'))
    array=np.ctypeslib.ndpointer(dtype=np.float64,ndim=1,flags='C_CONTIGUOUS')
    dll.evaluate.argtypes=[array,array,ctypes.c_int,ctypes.c_double,ctypes.c_double,ctypes.c_int]
    dll.evaluate.restype=None
    probes=[]
    for neutral,base,variants in [('T001','T002',['T027','T028']),('T009','T010',['T029','T030'])]:
        rate,n=audio(ROOT/'renders'/(neutral+'.wav'))
        _,b=audio(ROOT/'renders'/(base+'.wav'))
        for variant in variants:
            _,v=audio(ROOT/'renders'/(variant+'.wav'))
            for start in [6,12,19,32,42]:
                a=round((start-.1)*rate);z=round((start+.1)*rate)
                inp=np.ascontiguousarray(n[a:z,0]);target=np.ascontiguousarray(np.maximum(0,(20*np.log10(np.maximum(abs(inp),1e-15))+27.5)*.5))
                energy=lambda x:np.mean(x.reshape(-1,48)**2,axis=1)
                observed=10*np.log10(np.maximum(energy(v[a:z,0]),1e-30)/np.maximum(energy(b[a:z,0]),1e-30))
                probes.append(dict(id=variant,bite=5 if variant in ['T027','T029'] else 10,start=start,
                    inp=inp,target=target,observed=observed))
    def predict(p,tau,domain):
        out=np.empty(len(p['inp']));base=np.empty_like(out)
        dll.evaluate(p['target'],base,len(out),48000,.02,0)
        dll.evaluate(p['target'],out,len(out),48000,tau,domain)
        en=lambda g:np.mean((p['inp']*10**(-g/20)).reshape(-1,48)**2,axis=1)
        return 10*np.log10(np.maximum(en(out),1e-30)/np.maximum(en(base),1e-30))
    results=[]
    for domain in [0,1,2,3]:
        for bite in [5,10]:
            training=[p for p in probes if p['bite']==bite and p['start']==6]
            def loss(logtau):return np.mean([np.mean((predict(p,np.exp(logtau),domain)[100:150]-p['observed'][100:150])**2) for p in training])
            fit=minimize_scalar(loss,bounds=(np.log(.02),np.log(20)),method='bounded')
            tau=float(np.exp(fit.x));checks=[]
            for p in probes:
                if p['bite']!=bite:continue
                pred=predict(p,tau,domain);m=slice(100,150)
                checks.append(dict(id=p['id'],start_s=p['start'],rmse_relief_db=float(np.sqrt(np.mean((pred[m]-p['observed'][m])**2))),
                    observed_peak=float(np.max(p['observed'][m])),predicted_peak=float(np.max(pred[m])),
                    settled_relief_db=float(np.mean(pred[170:190]))))
            results.append(dict(domain=domain,bite=bite,attack_ms=tau,train_rmse_db=float(np.sqrt(fit.fun)),checks=checks))
            print(domain,bite,tau,np.sqrt(fit.fun),flush=True)
    save('auto_bite_candidate_fit.json',results)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--build',action='store_true')
    main(parser.parse_args().build)
