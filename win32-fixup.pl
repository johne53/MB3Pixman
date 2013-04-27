#! e:/program files/perl/bin/perl.exe
#  version info can be found in 'pixman-version.h'

require "../local-paths.lib";

$major = 0;
$minor = 29;
$micro = 3;
$binary_age = 29;
$interface_age = 0;
$current_minus_age = 1;

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
	    print OUTPUT;
	}
}

process_file ("pixman/pixman-version.h");

my $command=join(' ',@ARGV);
if ($command eq -buildall) {
	process_file ("msvc/pixman.vsprops");
}