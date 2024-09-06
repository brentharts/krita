#!/usr/bin/env python3
import os, sys, subprocess
if not os.path.isdir('./build'):
	os.mkdir('./build')

if '--install' in sys.argv:
	#os.system('sudo apt-get build-dep krita')
	os.system('sudo apt-get install libraqm-dev libexiv2-dev libquazip5-dev libmypaint-dev libqt5svg5-dev libkf5itemmodels-dev libkf5config-dev libkdecorations2-dev libqt5x11extras5-dev qtdeclarative5-dev libkf5guiaddons-dev libkf5configwidgets-dev libkf5windowsystem-dev libkf5coreaddons-dev')


cmd = ['cmake', os.path.abspath('.')]
print(cmd)
subprocess.check_call(cmd, cwd='./build')
