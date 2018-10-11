{
  'targets': [
    {
      'target_name': 'IPC',
      'sources': [ 'addon.cpp', 'ConnectionWrap.cpp', 'MessageWrap.cpp', '../common/hashtable.c' ],

      'include_dirs': [
        "<!(node -e \"require('nan')\")",
        "../cpp",
        "../src",
        "../common"
      ],

      'xcode_settings': {
        'MACOSX_DEPLOYMENT_TARGET': '10.9',
      },

      'libraries': ['-L<!(pwd)/../bin/cpp', '-Xlinker', '-rpath', '-Xlinker', '<!(pwd)/../bin/cpp', '-lIPC_CPP']
    }
  ]
}
