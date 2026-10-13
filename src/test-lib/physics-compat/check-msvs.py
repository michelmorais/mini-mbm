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

"""Check MSVS source manifests against the unified backend; does not invoke MSBuild."""
import json
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
NS = {'m': 'http://schemas.microsoft.com/developer/msbuild/2003'}


def sources(project):
    tree = ET.parse(project)
    result = {}
    for item in tree.findall('.//m:ClCompile[@Include]', NS):
        path = (project.parent / item.attrib['Include'].replace('\\', '/')).resolve()
        assert path.is_file(), path
        assert path not in result, path
        result[path] = item
    filters = ET.parse(str(project) + '.filters')
    filtered = {(project.parent / item.attrib['Include'].replace('\\', '/')).resolve()
                for item in filters.findall('.//m:ClCompile[@Include]', NS)}
    assert filtered == set(result), 'Project/filter source lists differ'
    return tree, result


def main():
    project = ROOT / 'platform-msvs/box2d/box2d.vcxproj'
    tree, items = sources(project)
    native = set((ROOT / 'third-party/box2d-2.4.1/src').rglob('*.cpp'))
    wrapper = {ROOT / 'plugins/box2d' / name for name in (
        'box2d-lua.cpp', 'box-2d-wrap.cpp', 'physics-box-2d-lua.cpp',
        'physics-box2d-joint-lua.cpp', 'physics-box-2d-fluid-lua.cpp',
        'physics-box-2d-fluid-create-lua.cpp')}
    expected = native | wrapper | {project.parent / 'dllmain-box-2d.cpp'}
    assert set(items) == expected, set(items) ^ expected
    optional = {p for p in expected if 'particle' in p.parts or 'fluid' in p.name}
    for path, item in items.items():
        excluded = item.find('m:ExcludedFromBuild', NS)
        if path in optional:
            assert excluded is not None and excluded.text == 'true'
            assert excluded.attrib['Condition'] == "'$(MbmBox2dParticles)'!='1'"
        else:
            assert excluded is None, path
    definitions = tree.findall('.//m:PreprocessorDefinitions', NS)
    assert len(definitions) == 4
    assert all('MBM_BOX2D_PARTICLES=$(MbmBox2dParticles)' in node.text for node in definitions)
    facade = ROOT / 'platform-msvs/box2d-liquid-fun/box2d-liquid-fun.vcxproj'
    tree, items = sources(facade)
    assert set(items) == {ROOT / 'plugins/box2d/box2d-compat-lua.cpp',
                          facade.parent / 'dllmain-box2d-liquid-fun.cpp'}
    reference = tree.find('.//m:ProjectReference', NS)
    assert reference is not None and reference.attrib['Include'] == '..\\box2d\\box2d.vcxproj'
    assert 'BOX_2D_BUILD_DLL' not in facade.read_text(), 'Facade must import the backend'
    print(json.dumps({'status': 'manifest_pass', 'on_sources': len(expected),
                      'off_sources': len(expected - optional), 'facade_sources': len(items),
                      'native_msbuild': 'not_run'}, indent=2))


if __name__ == '__main__':
    main()
