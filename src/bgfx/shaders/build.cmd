@echo off
set SHADERC=shadercRelease.exe
set VSP=--profile vs_4_0
set FSP=--profile ps_4_0

::set VSP=--profile spirv
::set FSP=--profile spirv

%SHADERC% --debug -f simple.frag -o simple_fs_bgfx.inc --bin2c --type fragment --platform windows %FSP%
%SHADERC% --debug -f default.vert -o default_vs_bgfx.inc --bin2c --type v --platform windows %VSP%
%SHADERC% --debug -f im2d.vert -o im2d_vs_bgfx.inc --bin2c --type v --platform windows %VSP%
%SHADERC% --debug -f im3d.vert -o im3d_vs_bgfx.inc --bin2c --type v --platform windows %VSP%
%SHADERC% --debug -f matfx_env.vert -o matfx_env_vs_bgfx.inc --bin2c --type v --platform windows %VSP%
%SHADERC% --debug -f matfx_env.frag -o matfx_env_fs_bgfx.inc --bin2c --type f --platform windows %FSP%