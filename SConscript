target = 'NSxfer'

files = Split("""
	gui.c
	main.c
	queue.c
	thread.c
	utils.c
	nsiswapi/pluginapi.c
""")

resources = Split("""
	resource.rc
""")

libs = Split("""
	kernel32
	advapi32
	ole32
	uuid
	user32
	shlwapi
	version
	wininet
""")

examples = ''

docs = Split("""
	NSxfer.Readme.txt
""")

Import('BuildPlugin')

BuildPlugin(target, files, libs, examples, docs, res = resources)