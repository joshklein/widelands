#!/bin/sh
echo " "
echo "###########################################################"
echo "#     Script to simplify the compilation of Widelands     #"
echo "###########################################################"
echo " "
echo "  Because of the many different systems Widelands"
echo "  might be compiled on, we unfortunally can not"
echo "  provide a simple way to prepare your system for"
echo "  compilation. To ensure your system is ready, best"
echo "  check http://wl.widelands.org/wiki/BuildingWidelands"
echo " "
echo "  You will often find helpful hands at our"
echo "  * IRC Chat: http://wl.widelands.org/webchat/"
echo "  * Forums: http://wl.widelands.org/forum/"
echo "  * Mailinglist: http://wl.widelands.org/wiki/MailLists/"
echo " "
echo "  Please post your bug reports and feature requests at:"
echo "  https://bugs.launchpad.net/widelands"
echo " "
echo "###########################################################"
echo " "
print_help () {
    # Print help for our options
    echo "Per default, this script will create a full debug build."
    echo "Unless explicitly switched off, AddressSanitizer will"
    echo "be used as well with debug builds."
    echo " "
    echo "The following options are available:"
    echo " "
    echo "-h or --help          Print this help."
    echo " "
    echo " "
    echo "Omission options:"
    echo " "
    echo "-w or --no-website    Omit building of website binaries."
    echo " "
    echo "-t or --no-translations"
    echo "                      Omit building translations."
    echo " "
    echo "-a or --no-asan       If in debug mode, switch off the AddressSanitizer."
    echo "                      Release builds are created without AddressSanitizer"
    echo "                      per default."
    echo " "
    echo "Compiler options:"
    echo " "
    echo "-r or --release       Create a release build. If this is not set,"
    echo "                      a debug build will be created."
    echo " "
    echo "--gcc                 Try to build with GCC rather than the system default."
    echo "                      If you built with Clang before, you will have to clean"
    echo "                      your build directory before switching compilers."
    echo "                      Expects that the compiler is in '/usr/bin/'."
    echo " "
    echo "--clang               Try to build with Clang rather than the system default."
    echo "                      If you built with GCC before, you will have to clean"
    echo "                      your build directory before switching compilers."
    echo "                      Expects that the compiler is in '/usr/bin/'."
    echo " "
    echo "For the AddressSanitizer output to be useful, some systems (e.g. Ubuntu Linux)"
    echo "require that you set a symlink to the symbolizer. For example:"
    echo " "
    echo "    sudo ln -s /usr/bin/llvm-symbolizer-3.8 /usr/bin/llvm-symbolizer"
    echo " "
    echo "More info about AddressSanitizer at:"
    echo " "
    echo "    http://clang.llvm.org/docs/AddressSanitizer.html"
    echo " "
    return
  }



## Options to control the build.
BUILD_WEBSITE="ON"
BUILD_TRANSLATIONS="ON"
BUILD_TYPE="Debug"
USE_ASAN="ON"
COMPILER="default"
while [ "$1" != "" ]; do
  if [ "$1" = "--no-website" -o "$1" = "-w" ]; then
    BUILD_WEBSITE="OFF"
  elif [ "$1" = "--release" -o "$1" = "-r" ]; then
    BUILD_TYPE="Release"
    USE_ASAN="OFF"
  elif [ "$1" = "--no-translations" -o "$1" = "-t" ]; then
    BUILD_TRANSLATIONS="OFF"
  elif [ "$1" = "--no-asan" -o "$1" = "-a" ]; then
    USE_ASAN="OFF"
  elif [ "$1" = "--gcc" ]; then
    if [ -f /usr/bin/gcc -a /usr/bin/g++ ]; then
      export CC=/usr/bin/gcc
      export CXX=/usr/bin/g++
    fi
  elif [ "$1" = "--clang" ]; then
    if [ -f /usr/bin/clang -a /usr/bin/clang++ ]; then
      export CC=/usr/bin/clang
      export CXX=/usr/bin/clang++
    fi
  elif [ "$1" = "--help" -o "$1" = "-h" ]; then
    print_help
    exit 0
  fi
  shift
done

if [ $BUILD_WEBSITE = "ON" ]; then
  echo "A complete build will be created."
  echo "You can use -w or --no-website to omit building and"
  echo "linking website-related executables."
else
  echo "Any website-related code will be OMITTED in the build."
  echo "Make sure that you have created and tested a full"
  echo "build before submitting code to the repository!"
fi
echo " "
if [ $BUILD_TRANSLATIONS = "ON" ]; then
  echo "Translations will be built."
  echo "You can use -t or --no-translations to omit building them."
else
echo "Translations will not be built."
fi
echo " "
echo "###########################################################"
echo " "
if [ $BUILD_TYPE = "Release" ]; then
  echo "Creating a Release build."
else
  echo "Creating a Debug build. Use -r to create a Release build."
fi
echo " "
if [ $USE_ASAN = "ON" ]; then
  echo "Will build with AddressSanitizer."
  echo "http://clang.llvm.org/docs/AddressSanitizer.html"
  echo "You can use -a or --no-asan to switch it off."
else
  echo "Will build without AddressSanitizer."
fi
echo " "
echo "###########################################################"
echo " "
echo "Call 'compile.sh -h' or 'compile.sh --help' for help."
echo ""
echo "For instructions on how to adjust options and build with"
echo "CMake, please take a look at"
echo "https://wl.widelands.org/wiki/BuildingWidelands/."
echo " "
echo "###########################################################"
echo " "

######################################
# Definition of some local variables #
######################################
buildtool="" #Use ninja by default, fall back to make if that is not available.
######################################


######################################
#    Definition of some functions    #
######################################
  # Check basic stuff
  basic_check () {
    # Check whether the script is run in a widelands main directory
    if ! [ -f src/wlapplication.cc ] ; then
      echo "  This script must be run from the main directory of the widelands"
      echo "  source code."
      exit 1
    fi
    return 0
  }

  set_buildtool () {
    #Defaults to ninja, but if that is not found, we use make instead
    if [ `command -v ninja` ] ; then
      buildtool="ninja"
    #On some systems (most notably Fedora), the binary is called ninja-build
    elif [ `command -v ninja-build` ] ; then
      buildtool="ninja-build"
    #... and some systems refer to GNU make as gmake
    elif [ `command -v gmake` ] ; then
      buildtool="gmake"
    else
      buildtool="make"
    fi
  }

  # Check if directories / links already exists and create / update them if needed.
  prepare_directories_and_links () {
    test -d build/locale || mkdir -p build/locale
    test -e data/locale || ln -s ../build/locale data/locale
    return 0
  }

  # Compile Widelands
  compile_widelands () {
    if [ $buildtool = "ninja" ] || [ $buildtool = "ninja-build" ] ; then
      cmake -G Ninja .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DOPTION_BUILD_WEBSITE_TOOLS=$BUILD_WEBSITE -DOPTION_BUILD_TRANSLATIONS=$BUILD_TRANSLATIONS -DOPTION_ASAN=$USE_ASAN
    else
      cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DOPTION_BUILD_WEBSITE_TOOLS=$BUILD_WEBSITE -DOPTION_BUILD_TRANSLATIONS=$BUILD_TRANSLATIONS -DOPTION_ASAN=$USE_ASAN
    fi

    if [ $(uname) == "Darwin" ]; then
      $buildtool -j "$(expr $(sysctl -n hw.ncpu) - 1)"
    else
      $buildtool -j "$(nproc --ignore=1)"
    fi

    return 0
  }

  # Remove old and move newly compiled files
  move_built_files () {
    rm  -f ../VERSION || true
    rm  -f ../widelands || true

    rm  -f ../wl_map_object_info || true
    rm  -f ../wl_map_info || true

    cp VERSION ../VERSION
    mv src/widelands ../widelands

    if [ $BUILD_WEBSITE = "ON" ]; then
        mv ../build/src/website/wl_map_object_info ../wl_map_object_info
        mv ../build/src/website/wl_map_info ../wl_map_info
    fi
    return 0
  }

  create_update_script () {
    # First check if this is an bzr checkout at all - only in that case,
    # creation of a script makes any sense.
    if ! [ -f .bzr/branch-format ] ; then
      echo "You don't appear to be using Bazaar. An update script will not be created"
      return 0
    fi
      rm -f update.sh || true
      cat > update.sh << END_SCRIPT
#!/bin/sh
echo "################################################"
echo "#            Widelands update script.          #"
echo "################################################"
echo " "

set -e
if ! [ -f src/wlapplication.cc ] ; then
  echo "  This script must be run from the main directory of the widelands"
  echo "  source code."
  exit 1
fi

bzr pull
cd build
$buildtool
rm  ../VERSION || true
rm  ../widelands || true
cp VERSION ../VERSION
mv src/widelands ../widelands
cd ..

echo " "
echo "################################################"
echo "#      Widelands was updated successfully.     #"
echo "# You should be able to run it via ./widelands #"
echo "################################################"
END_SCRIPT
      chmod +x ./update.sh
      echo "  -> The update script has successfully been created."
  }
######################################



######################################
#    Here is the "main" function     #
######################################
set -e
basic_check
set_buildtool
prepare_directories_and_links
mkdir -p build
cd build
compile_widelands
move_built_files
cd ..
create_update_script
echo " "
echo "###########################################################"
echo "# Congratulations! Widelands has been built successfully  #"
echo "# with the following settings:                            #"
echo "#                                                         #"
if [ $BUILD_TYPE = "Release" ]; then
  echo "# - Release build                                         #"
else
  echo "# - Debug build                                           #"
fi
if [ $BUILD_TRANSLATIONS = "ON" ]; then
  echo "# - Translations                                          #"
else
  echo "# - No translations                                       #"
fi

if [ $BUILD_WEBSITE = "ON" ]; then
  echo "# - Website-related executables                           #"
else
  echo "# - No website-related executables                        #"
fi
echo "#                                                         #"
echo "# You should now be able to run Widelands via             #"
echo "# typing ./widelands + ENTER in your terminal             #"
echo "#                                                         #"
echo "# You can update Widelands via running ./update.sh        #"
echo "# in the same directory that you ran this script in.      #"
echo "###########################################################"
######################################
