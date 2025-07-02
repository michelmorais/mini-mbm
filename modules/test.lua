module_folder = '/home/michel/mini-mbm/bin/'
package.cpath = module_folder .. "?.so;" .. package.path

tiny_obj_loader = require "tiny_obj_loader"
tiny_obj_loader.tiny_parse("cube.obj")
