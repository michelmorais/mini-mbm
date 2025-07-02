#/bin/bash
#clang-tidy --list-checks -checks='*' | grep "modernize"

INCLUDES="-I../include/ -I../include/core_mbm -I../third-party/ -I../include/render -I../third-party/stb -I../include/lua-5.4 -I../third-party/lua-5.4.1"
CORE_HOME=$(pwd)
SRC="${CORE_HOME}/../src"
CORE_OPENGL=${SRC}/core_mbm
CORE_RENDER=${SRC}/render
THIRD_PARTY=${CORE_HOME}/../third-party
LUA_WRAP="${SRC}/lua-wrap"
MINIZ=${CORE_OPENGL}/miniz
DEF="-DUSE_OPENGL_ES -DUSE_DEPRECATED_2_MINOR -DUSE_SQLITE3 -DUSE_VR -D_DEBUG -DLUA_COMPAT_ALL -DLUA_ANSI -DUSE_EDITOR_FEATURES -std=c++11 -pthread"
BOX2D_INC=-I${THIRD_PARTY}/box2d-2.3.1
BULLET_3D_WRAP=-I${THIRD_PARTY}/bullet-2.84
INCLUDES="${INCLUDES} ${BOX2D_INC} ${BULLET_3D_WRAP}"

modernize_list='modernize-loop-convert,modernize-make-unique,modernize-pass-by-value,modernize-redundant-void-arg,modernize-replace-auto-ptr,modernize-shrink-to-fit,modernize-use-auto,modernize-use-default,modernize-use-nullptr,modernize-use-override'
modernize_list='modernize-use-auto'
#-checks='-*,modernize-use-override'

folder=${SRC}
modernize=""
EXT="cpp"

echo "Select which one fix/analyze:"
echo "1  - modernize-loop-convert"
echo "2  - modernize-make-unique"
echo "3  - modernize-pass-by-value"
echo "4  - modernize-redundant-void-arg"
echo "5  - modernize-replace-auto-ptr"
echo "6  - modernize-shrink-to-fit"
echo "7  - modernize-use-auto"
echo "8  - modernize-use-default"
echo "9  - modernize-use-nullptr"
echo "10 - modernize-use-override"
echo "11 - modernize-*"
echo "12 - performance-*"
echo "13 - clang-analyzer-*"

read value

case ${value} in
1) modernize="modernize-loop-convert" ;;
2) modernize="modernize-make-unique" ;;
3) modernize="modernize-pass-by-value" ;;
4) modernize="modernize-redundant-void-arg" ;;
5) modernize="modernize-replace-auto-ptr" ;;
6) modernize="modernize-shrink-to-fit" ;;
7) modernize="modernize-use-auto" ;;
8) modernize="modernize-use-default" ;;
9) modernize="modernize-use-nullptr" ;;
10)modernize="modernize-use-override" ;;
11)modernize="modernize-*" ;;
12)modernize="performance-*";;
13)modernize="clang-analyzer-*";;
*) echo "invalid option!"; exit 1 ;;
esac

echo "Fix? else (to evaluate)"
echo "y/n"
read value

FIX=""
if [[ "${value}" == "y" ]]; then
    FIX="-fix"
fi


doClangTidy()
{
    which_folder=${1}
    ext=${2}
    all=`find "${which_folder}" -regex ".*\.${ext}$"`
    for file_cpp in ${all}; do
        echo ""
        echo "${file_cpp} process? y/n"
        read opt
        if [[ "${opt}" == "y" ]]; then
            clang-tidy -checks=-*,${modernize} ${file_cpp} ${FIX} -- ${INCLUDES} ${DEF}
        fi
    done
}

doClangTidy ${folder} ${EXT}
