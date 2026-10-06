# Append to the official Alone2 makefile without editing upstream.
PROGRESS_ENGINE_OBJS = $(filter-out $O/Main.o $O/List.o $O/BenchCon.o $O/UpdateCallbackConsole.o $O/ExtractCallbackConsole.o $O/HashCon.o $O/Extract.o $O/ArchiveExtractCallback.o $O/Update.o $O/UpdateCallback.o $O/LoadCodecs.o $O/FilePathAutoRename.o $O/ZipUpdate.o,$(OBJS))
PROGRESS_PORT_OBJS = $(addprefix $(PROGRESS_WORK)/,Main.o List.o BenchCon.o UpdateCallbackConsole.o ExtractCallbackConsole.o HashCon.o Extract.o ArchiveExtractCallback.o Update.o UpdateCallback.o LoadCodecs.o FilePathAutoRename.o ZipUpdate.o NativeProgress.o NativeExtraction.o NativeOverwrite.o NativeFileState.o ArchiveTransferPort.o NativeMetadata.o AgentProxy.o NativeBenchmark.o NativeFolderUpdate.o NativeAgentSelection.o NativeTempFiles.o)
$(PROGRESS_WORK)/ZipUpdate.o: $(PROGRESS_OVERLAY)/CPP/7zip/Archive/Zip/ZipUpdate.cpp
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/LoadCodecs.o: $(PROGRESS_OVERLAY)/CPP/7zip/UI/Common/LoadCodecs.cpp
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/Main.o: $(PROGRESS_OVERLAY)/CPP/7zip/UI/Console/Main.cpp $(PROGRESS_OVERLAY)/CPP/7zip/UI/Console/ExtractCallbackConsole.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) -I$(PROGRESS_PORT_SOURCE) $<
$(PROGRESS_WORK)/%.o: $(PROGRESS_OVERLAY)/CPP/7zip/UI/Console/%.cpp $(PROGRESS_OVERLAY)/CPP/7zip/UI/Console/ExtractCallbackConsole.h $(PROGRESS_PORT_SOURCE)/NativeProgress.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) -I$(PROGRESS_PORT_SOURCE) $<
$(PROGRESS_WORK)/Extract.o: $(PROGRESS_OVERLAY)/CPP/7zip/UI/Common/Extract.cpp $(PROGRESS_PORT_SOURCE)/NativeProgress.h $(PROGRESS_PORT_SOURCE)/NativeAgentSelection.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) -I$(PROGRESS_PORT_SOURCE) $<
$(PROGRESS_WORK)/ArchiveExtractCallback.o: $(PROGRESS_OVERLAY)/CPP/7zip/UI/Common/ArchiveExtractCallback.cpp $(PROGRESS_PORT_SOURCE)/NativeExtraction.h $(PROGRESS_PORT_SOURCE)/NativeOverwrite.h $(PROGRESS_PORT_SOURCE)/NativeFileState.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) -I$(PROGRESS_PORT_SOURCE) $<
$(PROGRESS_WORK)/NativeExtraction.o: $(PROGRESS_PORT_SOURCE)/NativeExtraction.cpp $(PROGRESS_PORT_SOURCE)/NativeExtraction.h $(PROGRESS_PORT_SOURCE)/NativeFileState.h $(PROGRESS_PORT_SOURCE)/NativeProgress.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/NativeProgress.o: $(PROGRESS_PORT_SOURCE)/NativeProgress.cpp $(PROGRESS_PORT_SOURCE)/NativeProgress.h $(PROGRESS_SOURCE_ROOT)/CPP/7zip/UI/Common/Extract.h $(PROGRESS_SOURCE_ROOT)/CPP/7zip/UI/Common/HashCalc.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/NativeOverwrite.o: $(PROGRESS_PORT_SOURCE)/NativeOverwrite.cpp $(PROGRESS_PORT_SOURCE)/NativeOverwrite.h $(PROGRESS_PORT_SOURCE)/NativeProgress.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/NativeFileState.o: $(PROGRESS_PORT_SOURCE)/NativeFileState.cpp $(PROGRESS_PORT_SOURCE)/NativeFileState.h $(PROGRESS_PORT_SOURCE)/NativeProgress.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/FilePathAutoRename.o: $(PROGRESS_OVERLAY)/CPP/7zip/Common/FilePathAutoRename.cpp $(PROGRESS_PORT_SOURCE)/NativeFileState.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) -I$(PROGRESS_PORT_SOURCE) $<
$(PROGRESS_WORK)/NativeTempFiles.o: $(PROGRESS_PORT_SOURCE)/NativeTempFiles.cpp $(PROGRESS_PORT_SOURCE)/NativeTempFiles.h $(PROGRESS_PORT_SOURCE)/upstream/TempBrowseEnumerator.inc $(PROGRESS_PORT_SOURCE)/upstream/TempBrowseProperties.inc
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/Update.o: $(PROGRESS_OVERLAY)/CPP/7zip/UI/Common/Update.cpp $(PROGRESS_PORT_SOURCE)/ArchiveTransferPort.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) -I$(PROGRESS_PORT_SOURCE) $<
$(PROGRESS_WORK)/ArchiveTransferPort.o: $(PROGRESS_PORT_SOURCE)/ArchiveTransferPort.cpp $(PROGRESS_PORT_SOURCE)/ArchiveTransferPort.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/NativeMetadata.o: $(PROGRESS_PORT_SOURCE)/NativeMetadata.cpp $(PROGRESS_PORT_SOURCE)/NativeMetadata.h $(PROGRESS_PORT_SOURCE)/NativeAgentProperties.h $(PROGRESS_PORT_SOURCE)/NativeAgentSelection.h $(PROGRESS_PORT_SOURCE)/upstream/AgentProperties.inc $(PROGRESS_PORT_SOURCE)/upstream/CopyName.inc
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/NativeAgentSelection.o: $(PROGRESS_PORT_SOURCE)/NativeAgentSelection.cpp $(PROGRESS_PORT_SOURCE)/NativeAgentSelection.h $(PROGRESS_PORT_SOURCE)/NativeAgentProperties.h $(PROGRESS_PORT_SOURCE)/ArchiveSourceStamp.h $(PROGRESS_PORT_SOURCE)/upstream/AgentSelection.inc
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/NativeBenchmark.o: $(PROGRESS_PORT_SOURCE)/NativeBenchmark.cpp $(PROGRESS_PORT_SOURCE)/NativeBenchmark.h $(PROGRESS_PORT_SOURCE)/NativeProgress.h $(PROGRESS_PORT_SOURCE)/upstream/BenchmarkGuiMath.inc
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/NativeFolderUpdate.o: $(PROGRESS_PORT_SOURCE)/NativeFolderUpdate.cpp $(PROGRESS_PORT_SOURCE)/NativeFolderUpdate.h $(PROGRESS_PORT_SOURCE)/NativeZipMetadata.h $(PROGRESS_PORT_SOURCE)/NativeAgentSelection.h $(PROGRESS_PORT_SOURCE)/upstream/AgentCreateFolder.inc $(PROGRESS_PORT_SOURCE)/upstream/AgentItemUpdate.inc $(PROGRESS_PORT_SOURCE)/upstream/AgentComment.inc $(PROGRESS_PORT_SOURCE)/upstream/AgentUpdateOneFile.inc $(PROGRESS_PORT_SOURCE)/upstream/AgentUpdateStream.inc
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/UpdateCallback.o: $(PROGRESS_OVERLAY)/CPP/7zip/UI/Common/UpdateCallback.cpp $(PROGRESS_PORT_SOURCE)/NativeFolderUpdate.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) -I$(PROGRESS_PORT_SOURCE) $<
$(PROGRESS_WORK)/AgentProxy.o: $(PROGRESS_OVERLAY)/CPP/7zip/UI/Agent/AgentProxy.cpp
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) $<
$(PROGRESS_WORK)/7zz-progress: $(PROGRESS_ENGINE_OBJS) $(PROGRESS_PORT_OBJS)
	$(CXX) -o "$@" $(MY_ARCH_2) $(LDFLAGS) $(FLAGS_FLTO) $(LD_arch) $(LFLAGS_NOEXECSTACK) $(PROGRESS_ENGINE_OBJS) $(PROGRESS_PORT_OBJS) $(MY_LIBS) $(LIB2)
.PHONY: port_progress
port_progress: $(PROGRESS_WORK)/7zz-progress
$(PROGRESS_WORK)/MetadataFormatterTests.o: $(PROGRESS_PORT_SOURCE)/../tests/native-metadata-formatter.cpp $(PROGRESS_PORT_SOURCE)/NativeMetadata.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) -I$(PROGRESS_PORT_SOURCE) $<
$(PROGRESS_WORK)/7zip-metadata-tests: $(PROGRESS_ENGINE_OBJS) $(filter-out $(PROGRESS_WORK)/Main.o,$(PROGRESS_PORT_OBJS)) $(PROGRESS_WORK)/MetadataFormatterTests.o
	$(CXX) -o "$@" $(MY_ARCH_2) $(LDFLAGS) $(FLAGS_FLTO) $(LD_arch) $(LFLAGS_NOEXECSTACK) $^ $(MY_LIBS) $(LIB2)
.PHONY: port_metadata_test
port_metadata_test: $(PROGRESS_WORK)/7zip-metadata-tests
$(PROGRESS_WORK)/BenchmarkFormatterTests.o: $(PROGRESS_PORT_SOURCE)/../tests/native-benchmark-formatter.cpp $(PROGRESS_PORT_SOURCE)/NativeBenchmark.h
	$(CXX) $(filter-out -Weverything -Wfatal-errors,$(CXXFLAGS)) -o "$@" -I$(PROGRESS_SOURCE_ROOT) -I$(PROGRESS_PORT_SOURCE) $<
$(PROGRESS_WORK)/7zip-benchmark-tests: $(PROGRESS_ENGINE_OBJS) $(filter-out $(PROGRESS_WORK)/Main.o,$(PROGRESS_PORT_OBJS)) $(PROGRESS_WORK)/BenchmarkFormatterTests.o
	$(CXX) -o "$@" $(MY_ARCH_2) $(LDFLAGS) $(FLAGS_FLTO) $(LD_arch) $(LFLAGS_NOEXECSTACK) $^ $(MY_LIBS) $(LIB2)
.PHONY: port_benchmark_test
port_benchmark_test: $(PROGRESS_WORK)/7zip-benchmark-tests
