#!/usr/bin/env python
# encoding: utf-8

import os

APPNAME = 'libfsw_sensor'
VERSION = '1.0'

def options(ctx):
	ctx.load('gcc')

	ctx.add_option('--toolchain', action='store', default='', help='Set toolchain prefix')

	gr = ctx.add_option_group('libfsw_sensor options')
	gr.add_option('--install-fsw_sensor', action='store_true', help='Installs FSW_SENSOR headers and lib')

	gr.add_option('--enable-fat', action='store_true', help='Builds FatFS')
	gr.add_option('--enable-uffs', action='store_true', help='Builds UFFS')

def configure(ctx):
	ctx.env.CC = ctx.options.toolchain + "gcc"
	ctx.env.AR = ctx.options.toolchain + "ar"
	ctx.load('gcc')
	ctx.find_program(ctx.options.toolchain + 'size', var='SIZE')


	ctx.env.append_unique('FILES_FSW_SENSOR' , ['src/*.c'])
	ctx.write_config_header('include/conf_storage.h', top=True, remove=True)
	
def build(ctx):
	ctx(export_includes='include', name='fsw_sensor_include')
	if ctx.env.FILES_FSW_SENSOR:

		install_path = False
		if ctx.options.install_storage:
			install_path = '${PREFIX}/lib'
			ctx.install_files('${PREFIX}', ctx.path.ant_glob('include/*.h'), relative_trick=True)
			ctx.install_files('${PREFIX}/include', 'include/conf_fsw_sensor.h', cwd=ctx.bldnode)
			ctx.install_files('~work/NanoMindA712D/nanomind-pub/lib/libstorage/include', ctx.path.ant_glob('include/*.h'), relatie_trick=True) 

		ctx.stlib(source=ctx.path.ant_glob(ctx.env.FILES_FSW_SENSOR, excl=ctx.env.EXCLUDES_FSW_SENSOR), 
			target='fsw_sensor',
			includes = 'include',
			defines = ctx.env.DEFINES_FSW_SENSOR,
			export_includes='include',
			use='csp gomspace include',
			install_path = install_path,
		)

		if ctx.options.verbose > 0:
			ctx(rule='${SIZE} --format=berkeley ${SRC}', source='libfsw_sensor.a', name='fsw_sensor_size', always=True)
