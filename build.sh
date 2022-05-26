#!/bin/bash

unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     MACHINE=linux;;
    Darwin*)    MACHINE=macos;;
    CYGWIN*)    MACHINE=win;;
    MINGW*)     MACHINE=win;;
    *)          echo "unsupported architecture"; exit 1
esac

BASHSCRIPTDIR="$(cd "$(dirname "$0")" || exit; pwd)"
ROOTDIR=${BASHSCRIPTDIR}
SOURCEDIR=${BASHSCRIPTDIR}
BUILDDIR=${ROOTDIR}/_build/${MACHINE}
SOURCEDIR=${ROOTDIR}/apps
#SOURCE2DIR=${ROOTDIR}/libs
NBFILES=${BUILDDIR}/.nbfiles

displayusage() {
    echo " ================================================================================= "
    echo "|    Usage:                                                                       |"
    echo "| do_build.sh OPTS                                                                |"
    echo "|    available options [OPTS]:                                                    |"
    echo "| -b) --build)          automatically updates the build when necessary            |"
    echo "| -c) --clean)          removes build dirs                                        |"
    echo "| -d) --dry-run)        creates the make file without building                    |"
    echo "| -f) --force)          forces an update of the build                             |"
    echo "| -h) --help)           print this help                                           |"
    echo "| -m) --make)           performs make                                             |"
	echo "| -n) --nproc)          sets the number of parallel processing (default nproc -1) |"
    echo "| -t) --build-type)     specifies a different cmake build type (e.g. \"-t -Debug\") |"
    echo "| -w) --cmake-params)   specifies cmake options in quoted (e.g. \"-DVAR=value\")    |"
	echo "| [no arguments]        automatically updates the build when necessary            |"
    echo " ================================================================================= "
}

update_makefiles(){
    mkdir -p "${BUILDDIR}"
    cd "${BUILDDIR}" || exit

	if [ "${MACHINE}" == "macos" ]; then
        cmake "${ROOTDIR}" -DCMAKE_BUILD_TYPE="${BUILDTYPE:-Release}" ${CMAKEOPTS:+$CMAKEOPTS}
    elif [ "${MACHINE}" == "linux" ]; then
        cmake "${ROOTDIR}" -DCMAKE_BUILD_TYPE="${BUILDTYPE:-Release}" ${CMAKEOPTS:+$CMAKEOPTS}
    elif [ "${MACHINE}" == "win" ]; then
        cmake -A x64 "${ROOTDIR}" ${CMAKEOPTS:+$CMAKEOPTS}
    fi
    CMAKE_RET=$?
	if [ $CMAKE_RET -ne 0 ] ; then exit ${CMAKE_RET}; fi
    cd - || exit
    MAKEFILEUPDATED="TRUE"
}

build(){
    if [[ ! -f "${NBFILES}" ]] ; then mkdir -p "${BUILDDIR}";  echo 0 > "${NBFILES}" ; fi
    PREVFILES=$(<"${NBFILES}")
    CURRFILES1=$(find "${SOURCEDIR}" |  wc -l)
    #CURRFILES2=$(find "${SOURCE2DIR}" |  wc -l)
    CURRFILES=$((CURRFILES1 + CURRFILES2))

    if [[ "${MAKEFILEUPDATED}" == "TRUE" ]] || [[ "${CURRFILES}" -ne "${PREVFILES}" ]] ; then update_makefiles ; fi

	if [ "${MACHINE}" == "win" ]; then
		cmake --build "${BUILDDIR}" --config "${BUILDTYPE:-Release}" -- -m:"${NPROC}"
	else
		cmake --build "${BUILDDIR}" --config "${BUILDTYPE:-Release}" -j "${NPROC}"
	fi
    CMAKE_RET=$?
    echo "${CURRFILES}" > "${NBFILES}"
	if [ $CMAKE_RET -ne 0 ] ; then exit ${CMAKE_RET}; fi
}

emake(){
    cd "${BUILDDIR}" || exit ; make -j"${NPROC}"
}

for arg in "$@"; do
	shift
	case "$arg" in
		"--build")          set -- "$@" "-b" ;;
		"--clean")          set -- "$@" "-c" ;;
		"--dry-run")        set -- "$@" "-d" ;;
		"--force")          set -- "$@" "-f" ;;
		"--help")           set -- "$@" "-h" ;;
		"--make")           set -- "$@" "-m" ;;
		"--nproc")          set -- "$@" "-n" ;;
		"--build-type")     set -- "$@" "-t" ;;
		"--cmake-params")   set -- "$@" "-w" ;;
		*)                  set -- "$@" "$arg";;
	esac
done

# Parse short options
OPTIND=1
while getopts "bcdfhilmn:pqrt:w:?" opt
do
	case "$opt" in
		"b") build; exit 0;;
		"c") CLEANBUILD="TRUE";;
		"d") UPDATEMAKEFILES="TRUE"; DRY_RUN="TRUE" ;;
		"f") UPDATEMAKEFILES="TRUE";;
		"h") displayusage; exit 0;;
		"m") emake; exit 0;;
		"n") NPROC=${OPTARG};;
		"t") BUILDTYPE=${OPTARG}; UPDATEMAKEFILES="TRUE";;
		"w") CMAKEOPTS+="${OPTARG} "; UPDATEMAKEFILES="TRUE";;
        "?") displayusage; exit 0;;
    esac
done

shift "$((OPTIND-1))"

if [[ -z "${NPROC}" ]]; then
	if [ "${MACHINE}" == "win" ]; then
		(( NPROC = 3 ))
	else
		(( NPROC = $(nproc) - 1 ))
	fi
fi


if [[ "${CLEANBUILD}" == "TRUE" ]] ; then rm -rf _bin _build; fi

if [[ "${UPDATEMAKEFILES}" == "TRUE" ]] ; then update_makefiles; fi

if [[ "${DRY_RUN}" == "TRUE" ]] ; then  exit 0; fi

build

