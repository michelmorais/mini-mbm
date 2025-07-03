#update IMGUI
IMGUI_FOLDER=/home/michel/imgui # this is my folder, difine yours
#git clone https://github.com/ocornut/imgui /your/folder/imgui
MINI_MBM_HOME_IMGUI=/home/michel/mini-mbm/plugins/imGui # this is my folder, define yours

for file_name in $(find ${MINI_MBM_HOME_IMGUI} -name '*.h'); do
    f=`echo ${file_name} | sed  's/.*\///g'`
    source=`find ${IMGUI_FOLDER} -name ${f}`
    #meld ${source} ${f}
    cp -f ${source} ${f}
    echo "Copying ${source} ${f}"
done

for file_name in $(find ${MINI_MBM_HOME_IMGUI} -name '*.cpp'); do
    f=`echo ${file_name} | sed  's/.*\///g'`
    source=`find ${IMGUI_FOLDER} -name ${f}`
    #meld ${source} ${f}
    echo "Copying ${source} ${f}"
    cp -f ${source} ${f}
done