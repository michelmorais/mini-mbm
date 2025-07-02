#include <iostream>
#include <obj_importer_lua/obj_importer_lua.h>
#include <obj_importer_lua/OBJ_Loader.h>

#include <obj_importer_lua/tiny_obj_loader.h>


void printStack(lua_State *lua, const char *fileName, const unsigned int numLine)
{
    std::string stack("\n**********************************"
                        "\nState of stack at\n");
    int top = lua_gettop(lua);
    for (int i = 1, k = top; i <= top; i++, --k)
    {
        char str[255];
        int  type = lua_type(lua, i);
        sprintf(str, "\t%d| %8s |%d\n", -k, lua_typename(lua, type), i);
        stack += str;
    }
    stack += "**********************************\n\n";
    printf("%d:%s,%s", numLine, fileName, stack.c_str());
}

#ifndef DebugLuaStack 
	#define DebugLuaStack printStack(lua,__FILE__,__LINE__);
#endif

unsigned int color_unsigned_float(const float r,const float g, const float b)
{
    const unsigned int dwR = r >= 1.0f ? 0xff : r <= 0.0f ? 0x00 : static_cast<unsigned int>((r * 255.0f + 0.5f));
    const unsigned int dwG = g >= 1.0f ? 0xff : g <= 0.0f ? 0x00 : static_cast<unsigned int>((g * 255.0f + 0.5f));
    const unsigned int dwB = b >= 1.0f ? 0xff : b <= 0.0f ? 0x00 : static_cast<unsigned int>((b * 255.0f + 0.5f));

    return (dwR << 16) | (dwG << 8) | dwB;
}

static const char* getStringHexColorFromColor(const objl::Vector3 & rgb)
{
	static char color[12] = "";
	snprintf(color,sizeof(color)-1,"#%06x",color_unsigned_float(rgb.X,rgb.Y,rgb.Z));
	return color;
}

static const char* getStringHexColorFromColor(const float * rgb)
{
	static char color[12] = "";
	snprintf(color,sizeof(color)-1,"#%06x",color_unsigned_float(rgb[0],rgb[1],rgb[2]));
	return color;
}

static int parse(lua_State *lua)
{
    const char* file_name =  luaL_checkstring(lua,1);
	objl::Loader loader;
	const bool result = loader.LoadFile(file_name);
	if(result)
	{
		lua_settop(lua,0);
		lua_pushboolean(lua,1);
		std::cout << file_name << "  sucessfully parsed...\n";
		lua_newtable(lua);//frame
		for(unsigned int i = 0, index_subset = 1; i< loader.LoadedMeshes.size(); ++i, ++index_subset)
		{
			lua_newtable(lua);//tSubset

			lua_newtable(lua);//tVertex
			const auto & subset = loader.LoadedMeshes[i];
			for(unsigned int j =0, index_vertex = 1; j < subset.Vertices.size(); j++, ++index_vertex)
			{
				lua_newtable(lua);//raw
				const auto & vertex = subset.Vertices[j];
				lua_pushnumber(lua,vertex.Position.X);
				lua_setfield(lua, -2, "x");

				lua_pushnumber(lua,vertex.Position.Y);
				lua_setfield(lua, -2, "y");

				lua_pushnumber(lua,vertex.Position.Z);
				lua_setfield(lua, -2, "z");

				lua_pushnumber(lua,vertex.TextureCoordinate.X);
				lua_setfield(lua, -2, "u");

				lua_pushnumber(lua,vertex.TextureCoordinate.Y);
				lua_setfield(lua, -2, "v");

				lua_pushnumber(lua,vertex.Normal.X);
				lua_setfield(lua, -2, "nx");

				lua_pushnumber(lua,vertex.Normal.Y);
				lua_setfield(lua, -2, "ny");

				lua_pushnumber(lua,vertex.Normal.Z);
				lua_setfield(lua, -2, "nz");

				lua_rawseti(lua, -2, index_vertex);
			}
			lua_setfield(lua, -2, "tVertex");
			
			lua_newtable(lua);//tIndex
			for(unsigned int j =0, index_tIndex = 1; j < subset.Indices.size(); j++, ++index_tIndex)
			{
				const auto & iIndex = subset.Indices[j];
				lua_pushinteger(lua,iIndex+1);//one based
				lua_rawseti(lua, -2, index_tIndex);
			}
			lua_setfield(lua, -2, "tIndex");

			lua_newtable(lua);//tMaterial
			
			const auto & MeshMaterial = subset.MeshMaterial;
			lua_pushstring(lua,MeshMaterial.map_Kd.c_str());
			lua_setfield(lua, -2, "map_Kd");

			lua_pushstring(lua,MeshMaterial.map_Ka.c_str());
			lua_setfield(lua, -2, "map_Ka");

			lua_pushstring(lua,MeshMaterial.map_Ks.c_str());
			lua_setfield(lua, -2, "map_Ks");

			lua_pushstring(lua,MeshMaterial.map_bump.c_str());
			lua_setfield(lua, -2, "map_bump");

			lua_pushstring(lua,MeshMaterial.map_d.c_str());
			lua_setfield(lua, -2, "map_d");

			lua_pushstring(lua,MeshMaterial.map_Ns.c_str());
			lua_setfield(lua, -2, "map_Ns");

			lua_pushnumber(lua,MeshMaterial.d);
			lua_setfield(lua, -2, "d");

			lua_pushnumber(lua,MeshMaterial.illum);
			lua_setfield(lua, -2, "illum");

			lua_newtable(lua);
			lua_pushstring(lua,getStringHexColorFromColor(MeshMaterial.Ka));
			lua_setfield(lua, -2, "hex_color");
			lua_setfield(lua, -2, "Ka");

			lua_newtable(lua);
			lua_pushstring(lua,getStringHexColorFromColor(MeshMaterial.Kd));
			lua_setfield(lua, -2, "hex_color");
			lua_setfield(lua, -2, "Kd");

			lua_newtable(lua);
			lua_pushstring(lua,getStringHexColorFromColor(MeshMaterial.Ks));
			lua_setfield(lua, -2, "hex_color");
			lua_setfield(lua, -2, "Ks");

			lua_setfield(lua, -2, "tMaterial");
			
			lua_rawseti(lua, -2, index_subset);//insert as subset in the frame

		}
		return 2;
	}
	else
	{
		std::cout <<  "  Failed to parse file " << file_name << " ...\n";
		lua_pushboolean(lua,0);
		return 1;
	}
}

const std::string get_folder_from_file(const char * file_name)
{
	std::string base_file_name(file_name);
#if defined _WIN32
	const size_t p = base_file_name.find_last_of("\\");
#else
	const size_t p = base_file_name.find_last_of("/");
#endif
	if (std::string::npos != p)
		base_file_name.erase(p+1);
	else
		base_file_name.clear();
	return base_file_name;
}

static int tiny_parse(lua_State *lua)
{
    const char* file_name =  luaL_checkstring(lua,1);
	std::cout << "Parsing OBJ: [" << file_name << "]\n";

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string warn;
	std::string err;
	const bool triangulate = true;

	std::string base_file_name = get_folder_from_file(file_name);

	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, file_name,base_file_name.c_str(), triangulate,false);
	
	if (!warn.empty()) 
	{
		std::cout << "WARN: " << warn << std::endl;
	}

	if (!err.empty()) 
	{
		std::cerr << "ERR: " << err << std::endl;
	}

	if (!ret) 
	{
		std::cout << "Failed to load/parse [" << file_name << "]\n" ;
		lua_pushboolean(lua,0);
		return 1;
	}

	lua_settop(lua,0);
	lua_pushboolean(lua,1);
	std::cout << "Sucessfully parsed OBJ [" << file_name << "]\n" ;

	const bool hasNormal  = attrib.normals.size() > 0;
	const bool hasTexture = attrib.texcoords.size() > 0;
	
	lua_newtable(lua);//frame (2)
	for(unsigned int i = 0, index_subset = 1; i< shapes.size(); ++i, ++index_subset)
	{
		lua_newtable(lua);//tSubset (3)

		lua_newtable(lua);//tVertex (4)
		const auto & subset = shapes[i].mesh;
		
		for(unsigned int j = 0, index_vertex = 0, uv_index = 0; (j + 2) < attrib.vertices.size(); j+=3, ++index_vertex, uv_index += 2)
		{
			lua_newtable(lua);//raw (5)
			const auto & x   = attrib.vertices[j];
			const auto & y   = attrib.vertices[j+1];
			const auto & z   = attrib.vertices[j+2];

			lua_pushnumber(lua,x);//(6)
			lua_setfield(lua, 5, "x");

			lua_pushnumber(lua,y);//(5)
			lua_setfield(lua, 5, "y");

			lua_pushnumber(lua,z);//(5)
			lua_setfield(lua, 5, "z");

			if(hasTexture && (uv_index + 1) <  attrib.texcoords.size())
			{
				const auto & u   = attrib.texcoords[uv_index];
				const auto & v   = attrib.texcoords[uv_index+1];
				lua_pushnumber(lua,u);//(6)
				lua_setfield(lua, 5, "u");

				lua_pushnumber(lua,v);//(6)
				lua_setfield(lua, 5, "v");
			}

			if(hasNormal && (j + 2) <  attrib.normals.size())
			{
				const auto & nx   = attrib.normals[j];
				const auto & ny   = attrib.normals[j+1];
				const auto & nz   = attrib.normals[j+2];

				lua_pushnumber(lua,nx);//(6)
				lua_setfield(lua, 5, "nx");

				lua_pushnumber(lua,ny);//(6)
				lua_setfield(lua, 5, "ny");

				lua_pushnumber(lua,nz);//(6)
				lua_setfield(lua, 5, "nz");
			}

			/*if(index_vertex < subset.material_ids.size())
			{
				const int material_id = subset.material_ids[index_vertex];
				if(material_id != last_material_id)
				{
					last_material_id = material_id;
					lua_rawseti(lua, -2, index_vertex + 1);
					lua_setfield(lua, -2, "tVertex");

					lua_newtable(lua);//tVertex
				}
				
			}
			else*/
			{
				lua_rawseti(lua, 4, index_vertex + 1);// 5 -> 4 tVertex[index_vertex]
			}
		}
		lua_setfield(lua, 3, "tVertex");//4 -> 3 tSubset.tVertex
			
		lua_newtable(lua);//tIndex (4)
		for(unsigned int j = 0, index_tIndex = 1; j < subset.indices.size(); j++, ++index_tIndex)
		{
			const auto & iIndex   = subset.indices[j].vertex_index;
			lua_pushinteger(lua,iIndex+1);//one based (5)
			lua_rawseti(lua, 4, index_tIndex);// 5 -> 4
		}
		lua_setfield(lua, 3, "tIndex");// 4 -> 3 tSubset.tIndex

		if(materials.size() > 0)
		{
			lua_newtable(lua);//tMaterial (4)
			const auto & material = materials[0];
			lua_pushstring(lua,material.diffuse_texname.c_str());//(5)
			lua_setfield(lua, 4, "map_Kd");

			lua_pushstring(lua,material.ambient_texname.c_str());//(5)
			lua_setfield(lua, 4, "map_Ka");

			lua_pushstring(lua,material.specular_texname.c_str());//(5)
			lua_setfield(lua, 4, "map_Ks");

			lua_pushstring(lua,material.bump_texname.c_str());//(5)
			lua_setfield(lua, 4, "map_bump");

			lua_pushstring(lua,material.alpha_texname.c_str());//(5)
			lua_setfield(lua, 4, "map_d");

			lua_pushstring(lua,material.specular_highlight_texname.c_str());//(5)
			lua_setfield(lua, 4, "map_Ns");

			lua_pushnumber(lua,material.dissolve);//(5)
			lua_setfield(lua, 4, "d");

			lua_pushnumber(lua,material.illum);//(5)
			lua_setfield(lua, 4, "illum");

			lua_newtable(lua);//(5)
			lua_pushstring(lua,getStringHexColorFromColor(material.ambient));//(6)
			lua_setfield(lua, -2, "hex_color");
			lua_setfield(lua, -2, "Ka");

			lua_newtable(lua);//(5)
			lua_pushstring(lua,getStringHexColorFromColor(material.diffuse));//(6)
			lua_setfield(lua, -2, "hex_color");
			lua_setfield(lua, -2, "Kd");

			lua_newtable(lua);//(5)
			lua_pushstring(lua,getStringHexColorFromColor(material.specular));//(6)
			lua_setfield(lua, -2, "hex_color");
			lua_setfield(lua, -2, "Ks");

			lua_setfield(lua, 3, "tMaterial");
			
			lua_rawseti(lua, 2, index_subset);//insert as subset in the frame
		}
			

	}
	return 2;
}

static const luaL_Reg mylib[] =
{
    { "parse", parse},
	{ "tiny_parse", tiny_parse},
	
    { NULL, NULL }
};

//The name of this C function is the string
//"luaopen_" concatenated with a copy of the module name where each dot is replaced by an
//underscore. Moreover, if the module name has a hyphen, its prefix up to (and including) the
//first hyphen is removed. For instance, if the module name is a.v1-b.c, the function name
//will be luaopen_b_c.
//name of this function is not flexible
int luaopen_tiny_obj_loader (lua_State *lua)
{
	luaL_newlib(lua, mylib);
    return 1;
}
//sometimes it is followed by "lib" -> "lib"tiny_obj_loader
int luaopen_libtiny_obj_loader (lua_State *lua)
{
	return luaopen_tiny_obj_loader(lua);
}
