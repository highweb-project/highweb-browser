#!/usr/bin/python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Builds and packages CronetChromeWebView.framework.
"""

import argparse
import os
import shutil
import sys

def target_dir_name(build_config, target_device):
  """Returns a default output directory name string.

  Args:
    build_config: A string describing the build configuration. Ex: 'Debug'
    target_device: A string describing the target device. Ex: 'simulator'
  """
  return '%s-iphone%s' % (build_config, target_device)

def build(build_config, target_device, extra_gn_options):
  """Generates and builds CronetChromeWebView.framework.

  Args:
    build_config: A string describing the build configuration. Ex: 'Debug'
    target_device: A string describing the target device. Ex: 'simulator'
    extra_gn_options: A string of gn args (space separated key=value items) to
      be appended to the gn gen command.

  Returns:
    The return code of generating ninja if it is non-zero, else the return code
      of the ninja build command.
  """
  if target_device == 'os':
    target_cpu = 'arm'
    additional_cpu = 'arm64'
  else:
    target_cpu = 'x86'
    additional_cpu = 'x64'

  if build_config == 'Debug':
    build_config_gn_args = 'is_debug=true'
  else:
    build_config_gn_args = ('is_debug=false enable_stripping=true '
                            'is_official_build=true')

  build_dir = os.path.join("out", target_dir_name(build_config, target_device))
  gn_args = ('target_os="ios" enable_websockets=false '
            'is_component_build=false use_xcode_clang=true '
            'disable_file_support=true disable_ftp_support=true '
            'disable_brotli_filter=true ios_enable_code_signing=false '
            'enable_dsyms=true '
            'target_cpu="%s" additional_target_cpus = ["%s"] %s %s' %
            (target_cpu, additional_cpu, build_config_gn_args,
             extra_gn_options))

  gn_command = 'gn gen %s --args=\'%s\'' % (build_dir, gn_args)
  print gn_command
  gn_result = os.system(gn_command)
  if gn_result != 0:
    return gn_result

  ninja_command = ('ninja -C %s ios/web_view:cronet_ios_web_view_package' %
                   build_dir)
  print ninja_command
  return os.system(ninja_command)

def copy_build_products(build_config, target_device, out_dir):
  """Copies the resulting framework and symbols to out_dir.

  Args:
    build_config: A string describing the build configuration. Ex: 'Debug'
    target_device: A string describing the target device. Ex: 'simulator'
    out_dir: A string to the path which all build products will be copied.
  """
  target_dir = target_dir_name(build_config, target_device)
  build_dir = os.path.join("out", target_dir)

  # Copy framework.
  framework_source = os.path.join(build_dir, 'CronetChromeWebView.framework')
  framework_dest = os.path.join(out_dir, target_dir,
                                'CronetChromeWebView.framework')
  print 'Copying %s to %s' % (framework_source, framework_dest)
  shutil.copytree(framework_source, framework_dest)

  # Copy symbols.
  symbols_source = os.path.join(build_dir, 'CronetChromeWebView.dSYM')
  symbols_dest = os.path.join(out_dir, target_dir, 'CronetChromeWebView.dSYM')
  print 'Copying %s to %s' % (symbols_source, symbols_dest)
  shutil.copytree(symbols_source, symbols_dest)

def package_framework(build_config, target_device, out_dir, extra_gn_options):
  """Builds CronetChromeWebView.framework and copies the result to out_dir.

  Args:
    build_config: A string describing the build configuration. Ex: 'Debug'
    target_device: A string describing the target device. Ex: 'simulator'
    out_dir: A string to the path which all build products will be copied.
    extra_gn_options: A string of gn args (space separated key=value items) to
      be appended to the gn gen command.

  Returns:
    The return code of the build if it fails or 0 if the build was successful.
  """
  print '\nBuilding for %s (%s)' % (target_device, build_config)

  build_result = build(build_config, target_device, extra_gn_options)
  if build_result != 0:
    error = 'Building %s/%s failed with code: ' % (build_config, target_device)
    print >>sys.stderr, error, build_result
    return build_result
  copy_build_products(build_config, target_device, out_dir)
  return 0

def package_all_frameworks(out_dir, extra_gn_options):
  """Builds CronetChromeWebView.framework.

  Builds Release and Debug versions of CronetChromeWebView.framework for both
    iOS devices and simulator and copies the resulting frameworks into out_dir.

  Args:
    out_dir: A string to the path which all build products will be copied.
    extra_gn_options: A string of gn args (space separated key=value items) to
      be appended to the gn gen command.

  Returns:
    0 if all builds are successful or 1 if any build fails.
  """
  print 'Building CronetChromeWebView.framework...'

  # Package all builds in the output directory
  os.makedirs(out_dir)

  if package_framework('Debug', 'simulator', out_dir, extra_gn_options) != 0:
    return 1
  if package_framework('Debug', 'os', out_dir, extra_gn_options) != 0:
    return 1
  if package_framework('Release', 'simulator', out_dir, extra_gn_options) != 0:
    return 1
  if package_framework('Release', 'os', out_dir, extra_gn_options) != 0:
    return 1

  # Copy common files from last built package to out_dir.
  build_dir = os.path.join("out", target_dir_name('Release', 'os'))
  package_dir = os.path.join(build_dir, 'cronet_ios_web_view')
  shutil.copy2(os.path.join(package_dir, 'AUTHORS'), out_dir)
  shutil.copy2(os.path.join(package_dir, 'LICENSE'), out_dir)
  shutil.copy2(os.path.join(package_dir, 'VERSION'), out_dir)

  print '\nSuccess! CronetChromeWebView.framework is packaged into %s' % out_dir

  return 0

def main():
  description = "Build and package CronetChromeWebView.framework"
  parser = argparse.ArgumentParser(description=description)

  parser.add_argument('out_dir', nargs='?', default='out/CronetChromeWebView',
                      help='path to output directory')
  parser.add_argument('--no_goma', action='store_true',
                      help='Prevents adding use_goma=true to the gn args.')

  options, extra_options = parser.parse_known_args()
  print 'Options:', options

  if len(extra_options):
    print >>sys.stderr, 'Unknown options: ', extra_options
    return 1

  out_dir = options.out_dir
  # Make sure that the output directory does not exist
  if os.path.exists(out_dir):
    print >>sys.stderr, 'The output directory already exists: ' + out_dir
    return 1

  gn_options = '' if options.no_goma else 'use_goma=true'

  return package_all_frameworks(out_dir, gn_options)


if __name__ == '__main__':
  sys.exit(main())
