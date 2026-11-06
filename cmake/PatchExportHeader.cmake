# Run by FetchContent in kColorPicker's source directory, for the export
# header named by EXPORT_HEADER (kImageAnnotator, in third_party, has the
# same change made in its source). On Windows they
# define their export macro as nothing, leaving MinGW to export everything on
# its own; users then take a class's member functions from import stubs,
# whose addresses differ from the library's own. Qt finds a signal by
# comparing those addresses, so connecting to one of their signals by
# pointer fails ("signal not found"): kImageAnnotator never heard of a color
# picked in kColorPicker, and so could not keep it. Windows now gets
# dllexport/dllimport like everyone else.
file(READ ${EXPORT_HEADER} content)
string(REPLACE "#if defined(_WIN32)" "#if 0 // Glimpse: dllexport/dllimport on Windows too" content "${content}")
if(NOT content MATCHES "Glimpse: dllexport")
    message(FATAL_ERROR "PatchExportHeader: unexpected ${EXPORT_HEADER}")
endif()
file(WRITE ${EXPORT_HEADER} "${content}")
