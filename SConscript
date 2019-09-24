from building import *

cwd = GetCurrentDir()
path = [cwd + '/inc']
src  = Glob('src/*.c')

# Air720
if GetDepend(['PPP_DEVICE_USING_AIR720']):
    path += [cwd + '/class/air720']
    src += Glob('class/air720/ppp_device_air720.c')
    if GetDepend(['PPP_DEVICE_AIR720_SAMPLE']):
        src += Glob('samples/ppp_sample_air720.c')

# M6312
if GetDepend(['PPP_DEVICE_USING_M6312']):
    path += [cwd + '/class/m6312']
    src += Glob('class/m6312/ppp_device_m6312.c')
    if GetDepend(['PPP_DEVICE_M6312_SAMPLE']):
        src += Glob('samples/ppp_sample_m6312.c')

group = DefineGroup('ppp_device', src, depend = ['PKG_USING_PPP_DEVICE'], CPPPATH = path)

Return('group')
