# Add to the official Alone2 makefile; do not modify upstream sources.
COMMENT_ENGINE_OBJS = $(filter-out $O/Main.o $O/ZipUpdate.o,$(OBJS))
$(COMMENT_ZIP_OBJECT): $(COMMENT_ZIP_SOURCE)
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(COMMENT_SOURCE_ROOT) $<
$(COMMENT_HELPER_OBJECT): $(COMMENT_HELPER_SOURCE) $(dir $(COMMENT_HELPER_SOURCE))NativeZipMetadata.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -I$(COMMENT_SOURCE_ROOT) $<
$(COMMENT_HELPER_BINARY): $(COMMENT_ENGINE_OBJS) $(COMMENT_HELPER_OBJECT) $(COMMENT_ZIP_OBJECT)
	$(CXX) -o "$@" $(MY_ARCH_2) $(LDFLAGS) $(FLAGS_FLTO) $(LD_arch) $(LFLAGS_NOEXECSTACK) $(COMMENT_ENGINE_OBJS) $(COMMENT_HELPER_OBJECT) $(COMMENT_ZIP_OBJECT) $(MY_LIBS) $(LIB2)
.PHONY: port_zip_comment
port_zip_comment: $(COMMENT_HELPER_BINARY)
