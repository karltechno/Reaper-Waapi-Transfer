from __future__ import print_function
import argparse
import sys
import os
import shutil
import subprocess

def copy_akautobahn(wwise_sdk_dir):
    # Copy AkAutobahn sources.
    path_to_akautobahn = os.path.join(wwise_sdk_dir, 'samples', 'WwiseAuthoringAPI', 'cpp', 'SampleClient', 'AkAutobahn')
    dest_akautobahn = os.path.join(sys.path[0], 'ext', 'AkAutobahn', 'AkAutobahn')
    print('Copying AkAutobahn from: {} to {}'.format(path_to_akautobahn, dest_akautobahn))
    
    # clean dest then copy.
    shutil.rmtree(dest_akautobahn)
    shutil.copytree(path_to_akautobahn, dest_akautobahn)

def cmake_run(generator, platform, build_dir, cmake_vars):
    path_to_src = os.path.relpath(sys.path[0], build_dir)
    cmake_args = ['cmake', path_to_src, '-G', generator, '-A', platform]

    for k,v in cmake_vars.items():
        cmake_args.append('-D')
        cmake_args.append('{}={}'.format(k,v))

    try:
        os.makedirs(build_dir)
    except FileExistsError:
        pass

    os.chdir(build_dir)
    print('Running {}'.format(' '.join(cmake_args)))
    subprocess.run(cmake_args)
    

def main():
    # Find Wwise SDK dir.
    argparser = argparse.ArgumentParser(description='Create solution for reaper-waapi-transfer')
    argparser.add_argument('-wwise_sdk_root', required=False, help='Path to Wwise sdk (default: environment var {WWISESDK})')
    argparser.add_argument('-cmake_generator', help='CMake Generator (default: Visual Studio 15 2017).', default='Visual Studio 15 2017')
    argparser.add_argument('-cmake_generator_platform', help='CMake generator platform (default: x64).', default='x64')
    argparser.add_argument('-build_dir', help='Path to generate CMake solution into (default: build/).', default='build')

    argparser.add_argument('-reaper32_dir', help='Path to reaper32 directory (for debugging and automatically copying DLL).', required=False)
    argparser.add_argument('-reaper64_dir', help='Path to reaper64 directory (for debugging and automatically copying DLL).', required=False)

    args = argparser.parse_args()
    
    wwise_sdk_dir = args.wwise_sdk_root if args.wwise_sdk_root else os.getenv('WWISESDK')
    if not wwise_sdk_dir:
        print("WWISESDK is not set.")
        return 1

    if not os.path.isdir(wwise_sdk_dir):
        print('Wwise SDK path does not exist: {}'.format(wwise_sdk_dir))
        return 1

    print('Using Wwise SDK Dir: {}'.format(wwise_sdk_dir))

    copy_akautobahn(wwise_sdk_dir)
    cmake_vars = dict()

    cmake_vars['AKSDK_DIR'] = wwise_sdk_dir

    if args.reaper32_dir:
        cmake_vars['REAPER32_PATH'] = args.reaper32_dir

    if args.reaper64_dir:
        cmake_vars['REAPER64_PATH'] = args.reaper64_dir

    cmake_run(args.cmake_generator, args.cmake_generator_platform, args.build_dir, cmake_vars)

if __name__ == "__main__":
    sys.exit(main())