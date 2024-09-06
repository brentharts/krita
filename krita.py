#!/usr/bin/env python3
import os, sys, subprocess
if not os.path.isdir('./build'):
	os.mkdir('./build')

if '--install' in sys.argv:
	#os.system('sudo apt-get build-dep krita')
	## note: this is the wrong zug libzug-dev
	os.system('sudo apt-get install catch2 libunibreak-dev libraqm-dev libexiv2-dev libquazip5-dev libmypaint-dev libqt5svg5-dev libkf5itemmodels-dev libkf5config-dev libkdecorations2-dev libqt5x11extras5-dev qtdeclarative5-dev libkf5guiaddons-dev libkf5configwidgets-dev libkf5windowsystem-dev libkf5coreaddons-dev')

if not os.path.isdir('./zug'):
	cmd = ['git', 'clone', '--depth', '1', 'https://github.com/arximboldi/zug.git']
	print(cmd)
	subprocess.check_call(cmd)
	os.system('mkdir build-zug')
if not os.path.isdir('/usr/local/include/zug')
cmd = ['cmake', os.path.abspath('./zug')]
print(cmd)
subprocess.check_call(cmd, cwd='./build-zug')
subprocess.check_call(['sudo','make', 'install'], cwd='./build-zug')

if not os.path.isdir('./immer'):
	cmd = ['git', 'clone', '--depth', '1', 'https://github.com/arximboldi/immer.git']
	print(cmd)
	subprocess.check_call(cmd)
	os.system('mkdir build-immer')
cmd = ['cmake', os.path.abspath('./immer')]
print(cmd)
subprocess.check_call(cmd, cwd='./build-immer')
subprocess.check_call(['sudo','make', 'install'], cwd='./build-immer')

if not os.path.isdir('./lager'):
	cmd = ['git', 'clone', '--depth', '1', 'https://github.com/arximboldi/lager.git']
	print(cmd)
	subprocess.check_call(cmd)
	os.system('mkdir build-lager')
cmd = ['cmake', os.path.abspath('./lager')]
print(cmd)
subprocess.check_call(cmd, cwd='./build-lager')
subprocess.check_call(['sudo','make', 'install'], cwd='./build-lager')


cmd = ['cmake', os.path.abspath('.')]
print(cmd)
subprocess.check_call(cmd, cwd='./build')

cmd = ['make']
print(cmd)
subprocess.check_call(cmd, cwd='./build')
