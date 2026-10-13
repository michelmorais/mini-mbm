# /*-----------------------------------------------------------------------------------------------------------------------|
# | MIT License (MIT)                                                                                                      |
# | Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
# |                                                                                                                        |
# | Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
# | documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
# | the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
# | to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
# |                                                                                                                        |
# | The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
# | the Software.                                                                                                          |
# |                                                                                                                        |
# | THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
# | WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
# | COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
# | OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
# |                                                                                                                        |
# |-----------------------------------------------------------------------------------------------------------------------*/

from pathlib import Path
import subprocess,json,argparse
parser=argparse.ArgumentParser(description='Check Linux root CMake physics profiles without compiling.')
parser.add_argument('--build-root',type=Path,default=Path('build/physics-config-check'))
args_cli=parser.parse_args()
args_cli.build_root.mkdir(parents=True,exist_ok=True)
base=['cmake','-G','Unix Makefiles','-S','.', '-DPLAT=Linux','-DAUDIO=none','-DCMAKE_BUILD_TYPE=Debug','-DUSE_TEXTURE_MISSING_DIALOG=0','-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
cases={'default':([],False,True),'all':(['-DUSE_ALL=1'],True,True),'all-off':(['-DUSE_ALL=1','-DUSE_BOX2D_PARTICLES=OFF'],False,True),'missing-box2d':(['-DUSE_LUA=1','-DUSE_BOX2D_PARTICLES=ON'],True,False),'missing-lua':(['-DUSE_BOX2D=1','-DUSE_BOX2D_PARTICLES=ON'],True,False),'legacy-only':(['-DUSE_LUA=1','-DUSE_BOX2D_LIQUID_FUN=1'],False,True)}
cases.update({
 'unified-on': (['-DUSE_LUA=1','-DUSE_BOX2D=1','-DUSE_BOX2D_PARTICLES=ON'],True,True),
 'unified-off': (['-DUSE_LUA=1','-DUSE_BOX2D=1','-DUSE_BOX2D_PARTICLES=OFF'],False,True),
 'unified-old-flag': (['-DUSE_LUA=1','-DUSE_BOX2D=1','-DUSE_BOX2D_LIQUID_FUN=1'],False,True),
})
for name,(args,particles,success) in cases.items():
 d=args_cli.build_root/name
 p=subprocess.run(base+['-B',str(d),'-DMBM_OUTPUT_ROOT='+str((d/'output').resolve())]+args,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
 (args_cli.build_root/(name+'.log')).write_text(p.stdout)
 assert (p.returncode==0)==success,(name,p.stdout[-2000:])
 if success:
  cache=(d/'CMakeCache.txt').read_text()
  assert ('USE_BOX2D_PARTICLES:BOOL=ON' in cache)==particles,name
  cmds=json.loads((d/'compile_commands.json').read_text())
  sourcefiles=[c['file'] for c in cmds]
  if name.startswith(('all','unified')):
   assert not any('box2d-liquid-fun' in f for f in sourcefiles),name
   assert any(f.endswith('/box2d-compat-lua.cpp') for f in sourcefiles),name
   dependencies=(d/'CMakeFiles/Makefile2').read_text()
   show_target='\n'.join(line for line in dependencies.splitlines() if line.startswith('CMakeFiles/show_built.dir/all:'))
   assert 'plugins/box2d/CMakeFiles/box2dLiquidFun.dir/all' in show_target,name
   assert any('/src/particle/' in f for f in sourcefiles)==particles,name
  if name == 'legacy-only':
   assert any('box2d-liquid-fun-2.3.1' in f for f in sourcefiles),name
   assert not any('/box2d-2.4.1/' in f for f in sourcefiles),name
 else: assert 'USE_BOX2D_PARTICLES requires USE_BOX2D=1 and USE_LUA=1' in p.stdout,name
 print(name,'PASS')
