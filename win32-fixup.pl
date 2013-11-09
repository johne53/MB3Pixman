#! e:/program files/perl/bin/perl.exe
#  version info can be found in 'configure.ac'

require "../local-paths.lib";

$pixman_version = "0.31.3";
$major = 0;
$minor = 31;
$micro = 3;
$binary_age = 3103;
$interface_age = 0;
$current_minus_age = 3;
$exec_prefix = "lib";

sub process_file
{
        my $outfilename = shift;
	my $infilename = $outfilename . ".in";
	
	open (INPUT, "< $infilename") || exit 1;
	open (OUTPUT, "> $outfilename") || exit 1;
	
	while (<INPUT>) {
	    s/\@PIXMAN_VERSION_MAJOR\@/$major/g;
	    s/\@PIXMAN_VERSION_MINOR\@/$minor/g;
	    s/\@PIXMAN_VERSION_MICRO\@/$micro/g;
	    s/\@PIXMAN_BINARY_AGE\@/$binary_age/g;
	    s/\@PIXMAN_INTERFACE_AGE\@/$interface_age/g;
	    s/\@LT_CURRENT_MINUS_AGE\@/$current_minus_age/g;
	    s/\@PACKAGE_VERSION@/$pixman_version/g;
	    s/\@GlibBuildRootFolder@/$glib_build_root_folder/g;
	    s/\@CairoBuildProjectFolder@/$cairo_build_project_folder/g;
	    s/\@GenericIncludeFolder@/$generic_include_folder/g;
	    s/\@GenericLibraryFolder@/$generic_library_folder/g;
	    s/\@GenericWin32LibraryFolder@/$generic_win32_library_folder/g;
	    s/\@GenericWin32BinaryFolder@/$generic_win32_binary_folder/g;
	    s/\@Debug32TestSuiteFolder@/$debug32_testsuite_folder/g;
	    s/\@Release32TestSuiteFolder@/$release32_testsuite_folder/g;
	    s/\@Debug32TargetFolder@/$debug32_target_folder/g;
	    s/\@Release32TargetFolder@/$release32_target_folder/g;
	    s/\@TargetSxSFolder@/$target_sxs_folder/g;
	    s/\@prefix@/$prefix/g;
	    s/\@exec_prefix@/$exec_prefix/g;
	    s/\@includedir@/$generic_include_folder/g;
	    s/\@libdir@/$generic_library_folder/g;
	    print OUTPUT;
	}
}

process_file ("pixman/pixman-version.h");
process_file ("pixman-1.pc");

my $command=join(' ',@ARGV);
if ($command eq -buildall) {
	process_file ("msvc/pixman.vsprops");
}