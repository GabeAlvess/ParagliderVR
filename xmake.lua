set_xmakever('3.0.1')
includes('lib/commonlibsse-ng')

set_project('ParagliderVR')
set_version('0.100')
set_languages('c++23')
set_warnings('all')
set_policy('package.requires_lock', true)
set_toolset('msvc', 'ninja')
add_rules('mode.debug', 'mode.releasedbg', 'mode.release')

target('ParagliderVR')
    add_deps('commonlibsse-ng')
    add_rules('commonlibsse-ng.plugin', {
        name = 'ParagliderVR',
        author = 'Alves',
        description = 'Physical paraglider locomotion for Skyrim VR',
        runtime = 'vr'
    })
    add_files('Src/**.cpp')
    remove_files('Src/oar_api/oar_api/**.cpp')
    add_headerfiles('Src/**.h')
    add_includedirs('Src', '$(projectdir)')
    set_pcxxheader('Src/pch.h')
    add_defines('ENABLE_SKYRIM_VR')

    after_build(function (target)
        local output = path.join(os.projectdir(), 'install_output')
        local plugins = path.join(output, 'SKSE', 'Plugins')
        os.mkdir(plugins)
        os.cp(target:targetfile(), plugins)
        os.cp(path.join(os.projectdir(), 'Assets', 'meshes'), output)
        os.cp(path.join(os.projectdir(), 'Assets', 'textures'), output)
        os.cp(path.join(os.projectdir(), 'Assets', 'Sound'), output)
        os.cp(path.join(os.projectdir(), 'Assets', 'ParagliderVR.esp'), output)
        os.cp(path.join(os.projectdir(), 'Assets', 'SKSE', 'Plugins', 'ParagliderVR.ini'), plugins)
    end)
