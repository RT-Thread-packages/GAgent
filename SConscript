from building import *

cwd     = GetCurrentDir()
src     = Glob('*.c')

if GetDepend(['PKG_USING_GAGENT_CLOUD_EXAMPLE']):
	src += Glob('example/*.c')

CPPPATH = [cwd]

group = DefineGroup('gagent_cloud', src, depend = ['PKG_USING_GAGENT_CLOUD', 'RT_USING_LWIP','PKG_USING_WEBCLIENT', 'PKG_USING_PAHOMQTT'], CPPPATH = CPPPATH)

Return('group')
