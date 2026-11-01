from pathlib import Path
src=(Path(__file__).parent.parent/'source/pkg_install.c').read_text()
assert 'params.param.option=BGFT_OPT_DISABLE_CDN|BGFT_OPT_FORCE_UPDATE;' in src, 'Confirmed install should allow update over an existing title'
assert 'sceAppInstUtilAppUnInstall' not in src, 'Never silently uninstall an existing title'
assert 'BGFT_OPT_DELETE_AFTER_UPLOAD' not in src, 'Never delete the source PKG'
assert 'if(!bgft_init_usable(ri))' in src, 'Abort on failed BGFT initialization other than already initialized'
assert 'if(aiu_rc!=0)' in src, 'Abort on failed AppInstUtil initialization'
assert 'params.param.content_name=strrchr(path' in src, 'Use the actual PKG filename'
print('pkg installer safety checks passed')
