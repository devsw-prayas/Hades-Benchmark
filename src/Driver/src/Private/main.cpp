/*
* Copyright (c) 2026 StormWeaver
*
* This file is part of the Hades Benchmarking API
*
* Licensed under the MIT License. You may obtain a copy of the License at
* https://opensource.org/licenses/MIT
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND...
*/
#include "HadesDriver.h"
#include "Commands.h"

// Just a shell interpreter: owns zero parsing/generation logic of its own
// beyond argv dispatch - every real subcommand body lives in Commands.cpp
// and only ever touches Core's config readers + Codegen's writer interface.
namespace {

	void printUsage() {
		std::cout <<
			"Hades Driver " << HADES_VERSION << "\n"
			"usage: hades-driver <command> [args...]\n"
			"\n"
			"global flags:\n"
			"  --version               print the Hades version triplet\n"
			"  --help                  print this message\n"
			"\n"
			"commands:\n"
			"  init-suite <dir>        create a hades-gen/ suite and adopt it\n"
			"  find-suite <dir>        adopt an existing hades-gen/ suite\n"
			"  rm <dir>                delete a suite (and un-adopt it if currently adopted)\n"
			"  new-test <fixture> <id> [--header=] [--adapter=] [--chrono=] [--hash=] [--kind=]\n"
			"  run [--build=cmake] [--fbt=<pattern>] [--format=console|json]\n"
			"  validate [--build=cmake]\n";
	}

}

int main(int v_Argc, char** p_Argv) {
	std::vector<std::string> l_args(p_Argv + 1, p_Argv + v_Argc);

	if (l_args.empty() || l_args[0] == "--help") {
		printUsage();
		return 0;
	}
	if (l_args[0] == "--version") {
		std::cout << HADES_VERSION << "\n";
		return 0;
	}

	const std::string l_command = l_args[0];
	const std::vector<std::string> l_rest(l_args.begin() + 1, l_args.end());

	if (l_command == "init-suite") return Hades::Driver::cmdInitSuite(l_rest);
	if (l_command == "find-suite") return Hades::Driver::cmdFindSuite(l_rest);
	if (l_command == "rm")         return Hades::Driver::cmdRemoveSuite(l_rest);
	if (l_command == "new-test")   return Hades::Driver::cmdNewTest(l_rest);
	if (l_command == "run")        return Hades::Driver::cmdRun(l_rest);
	if (l_command == "validate")   return Hades::Driver::cmdValidate(l_rest);

	std::cerr << "error: unknown command '" << l_command << "'\n";
	printUsage();
	return 2;
}
