package OpenSSL::safe::installdata;

use strict;
use warnings;
use Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(
    @PREFIX
    @libdir
    @BINDIR @BINDIR_REL_PREFIX
    @LIBDIR @LIBDIR_REL_PREFIX
    @INCLUDEDIR @INCLUDEDIR_REL_PREFIX
    @APPLINKDIR @APPLINKDIR_REL_PREFIX
    @ENGINESDIR @ENGINESDIR_REL_LIBDIR
    @MODULESDIR @MODULESDIR_REL_LIBDIR
    @PKGCONFIGDIR @PKGCONFIGDIR_REL_LIBDIR
    @CMAKECONFIGDIR @CMAKECONFIGDIR_REL_LIBDIR
    $VERSION @LDLIBS
);

our @PREFIX                     = ( '/mnt/c/Users/Tycho/CLionProjects/BeatSaberSpotifySearch/third-party/openssl-3.5.2/build/android-arm64' );
our @libdir                     = ( '/mnt/c/Users/Tycho/CLionProjects/BeatSaberSpotifySearch/third-party/openssl-3.5.2/build/android-arm64/lib' );
our @BINDIR                     = ( '/mnt/c/Users/Tycho/CLionProjects/BeatSaberSpotifySearch/third-party/openssl-3.5.2/build/android-arm64/bin' );
our @BINDIR_REL_PREFIX          = ( 'bin' );
our @LIBDIR                     = ( '/mnt/c/Users/Tycho/CLionProjects/BeatSaberSpotifySearch/third-party/openssl-3.5.2/build/android-arm64/lib' );
our @LIBDIR_REL_PREFIX          = ( 'lib' );
our @INCLUDEDIR                 = ( '/mnt/c/Users/Tycho/CLionProjects/BeatSaberSpotifySearch/third-party/openssl-3.5.2/build/android-arm64/include' );
our @INCLUDEDIR_REL_PREFIX      = ( 'include' );
our @APPLINKDIR                 = ( '/mnt/c/Users/Tycho/CLionProjects/BeatSaberSpotifySearch/third-party/openssl-3.5.2/build/android-arm64/include/openssl' );
our @APPLINKDIR_REL_PREFIX      = ( 'include/openssl' );
our @ENGINESDIR                 = ( '/mnt/c/Users/Tycho/CLionProjects/BeatSaberSpotifySearch/third-party/openssl-3.5.2/build/android-arm64/lib/engines-3' );
our @ENGINESDIR_REL_LIBDIR      = ( 'engines-3' );
our @MODULESDIR                 = ( '/mnt/c/Users/Tycho/CLionProjects/BeatSaberSpotifySearch/third-party/openssl-3.5.2/build/android-arm64/lib/ossl-modules' );
our @MODULESDIR_REL_LIBDIR      = ( 'ossl-modules' );
our @PKGCONFIGDIR               = ( '/mnt/c/Users/Tycho/CLionProjects/BeatSaberSpotifySearch/third-party/openssl-3.5.2/build/android-arm64/lib/pkgconfig' );
our @PKGCONFIGDIR_REL_LIBDIR    = ( 'pkgconfig' );
our @CMAKECONFIGDIR             = ( '/mnt/c/Users/Tycho/CLionProjects/BeatSaberSpotifySearch/third-party/openssl-3.5.2/build/android-arm64/lib/cmake/OpenSSL' );
our @CMAKECONFIGDIR_REL_LIBDIR  = ( 'cmake/OpenSSL' );
our $VERSION                    = '3.5.2';
our @LDLIBS                     =
    # Unix and Windows use space separation, VMS uses comma separation
    $^O eq 'VMS'
    ? split(/ *, */, '-ldl -pthread ')
    : split(/ +/, '-ldl -pthread ');

1;
