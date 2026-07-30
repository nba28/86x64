import json, sys, glob, os
paths = sorted(glob.glob(os.path.expanduser('~/Library/Logs/DiagnosticReports/Civilization*.ips')),
               key=os.path.getmtime)
p = sys.argv[1] if len(sys.argv) > 1 else paths[-1]
print("file:", os.path.basename(p))
raw = open(p).read().split('\n', 1)
head = json.loads(raw[0])
d = json.loads(raw[1])
print("timestamp:", head.get('timestamp'))
print("proc path:", d.get('procPath'))
print("exception:", d.get('exception'))
print("termination:", d.get('termination'))
ft = d.get('faultingThread', 0)
th = d['threads'][ft]
imgs = d['usedImages']
print("faulting thread %d frames:" % ft)
for fr in th['frames'][:14]:
    im = imgs[fr['imageIndex']]
    print("   %-30s +0x%-9x %s" % (im.get('name'), fr.get('imageOffset', 0), fr.get('symbol', '')))
