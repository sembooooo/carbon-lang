// Part of the Carbon Language project, under the Apache License v2.0 with LLVM
// Exceptions. See /LICENSE for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "explorer/fuzzing/fuzzer_util.h"

#include <google/protobuf/text_format.h>

#include "common/check.h"
#include "common/error.h"
#include "common/fuzzing/proto_to_carbon.h"
#include "explorer/interpreter/exec_program.h"
#include "explorer/interpreter/trace_stream.h"
#include "explorer/syntax/parse.h"
#include "explorer/syntax/prelude.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Path.h"
#include "tools/cpp/runfiles/runfiles.h"

namespace Carbon {

auto GetRunfilesFile(const std::string& file) -> ErrorOr<std::string> {
  using bazel::tools::cpp::runfiles::Runfiles;
  std::string error;
  // `Runfiles::Create()` fails if passed an empty `argv0`.
  std::unique_ptr<Runfiles> runfiles(Runfiles::Create(
      /*argv0=*/llvm::sys::fs::getMainExecutable(nullptr, nullptr), &error));
  if (runfiles == nullptr) {
    return Error(error);
  }
  std::string full_path = runfiles->Rlocation(file);
  if (!llvm::sys::fs::exists(full_path)) {
    return ErrorBuilder() << full_path << " doesn't exist";
  }
  return full_path;
}

auto ParseAndExecute(const Fuzzing::Carbon& carbon) -> ErrorOr<int> {
  const std::string source = ProtoToCarbon(carbon, /*maybe_add_main=*/true);

  Arena arena;
  CARBON_ASSIGN_OR_RETURN(AST ast,
                          ParseFromString(&arena, "Fuzzer.carbon", source,
                                          /*parser_debug=*/false));
  const ErrorOr<std::string> prelude_path =
      GetRunfilesFile("carbon/explorer/data/prelude.carbon");
  // Can't do anything without a prelude, so it's a fatal error.
  CARBON_CHECK(prelude_path.ok()) << prelude_path.error();

  AddPrelude(*prelude_path, &arena, &ast.declarations,
             &ast.num_prelude_declarations);
  TraceStream trace_stream;

  // Use llvm::nulls() to suppress output from the Print intrinsic.
  CARBON_ASSIGN_OR_RETURN(
      ast, AnalyzeProgram(&arena, ast, &trace_stream, &llvm::nulls()));
  return ExecProgram(&arena, ast, &trace_stream, &llvm::nulls());
}

}  // namespace Carbon
