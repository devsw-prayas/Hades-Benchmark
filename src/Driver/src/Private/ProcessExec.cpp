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
#include "ProcessExec.h"

#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

namespace Hades::Driver {

	std::string quotePath(const std::string& v_Path) {
		if (!v_Path.empty() && v_Path.front() == '"') {
			return v_Path;
		}
		return "\"" + v_Path + "\"";
	}

	int runProcess(const std::string& v_Command) {
		return std::system(v_Command.c_str());
	}

#ifdef _WIN32
	ProcessResult runProcessTimed(const std::string& v_Command, unsigned v_TimeoutSeconds, const std::string& v_CaptureFilePath) {
		ProcessResult l_result;

		SECURITY_ATTRIBUTES l_inheritable{};
		l_inheritable.nLength = sizeof(l_inheritable);
		l_inheritable.bInheritHandle = TRUE;

		// Redirect target for both stdout and stderr - overwritten every call
		// (isolate mode runs fixtures sequentially, one capture file at a time).
		const HANDLE l_captureFile = ::CreateFileA(
			v_CaptureFilePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &l_inheritable,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (l_captureFile == INVALID_HANDLE_VALUE) {
			l_result.m_ExitCode = -1;
			return l_result;
		}

		// NUL device for stdin - fixtures never read it, and STARTF_USESTDHANDLES
		// requires all three standard handles to be explicitly set.
		const HANDLE l_nulInput = ::CreateFileA(
			"NUL", GENERIC_READ, FILE_SHARE_READ, &l_inheritable,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		STARTUPINFOA l_startupInfo{};
		l_startupInfo.cb = sizeof(l_startupInfo);
		l_startupInfo.dwFlags = STARTF_USESTDHANDLES;
		l_startupInfo.hStdInput = l_nulInput;
		l_startupInfo.hStdOutput = l_captureFile;
		l_startupInfo.hStdError = l_captureFile;
		PROCESS_INFORMATION l_processInfo{};

		// CreateProcessA's lpCommandLine must be a writable buffer.
		std::string l_cmdLine = v_Command;

		const BOOL l_created = ::CreateProcessA(
			nullptr, l_cmdLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
			&l_startupInfo, &l_processInfo);

		::CloseHandle(l_captureFile);
		if (l_nulInput != INVALID_HANDLE_VALUE) {
			::CloseHandle(l_nulInput);
		}

		if (!l_created) {
			l_result.m_ExitCode = -1;
			return l_result;
		}

		const DWORD l_waitMs = (v_TimeoutSeconds == 0) ? INFINITE : (v_TimeoutSeconds * 1000u);
		const DWORD l_waitStatus = ::WaitForSingleObject(l_processInfo.hProcess, l_waitMs);

		if (l_waitStatus == WAIT_TIMEOUT) {
			::TerminateProcess(l_processInfo.hProcess, static_cast<UINT>(-1));
			::WaitForSingleObject(l_processInfo.hProcess, INFINITE);
			l_result.m_TimedOut = true;
		} else {
			DWORD l_exitCode = 0;
			::GetExitCodeProcess(l_processInfo.hProcess, &l_exitCode);
			l_result.m_ExitCode = static_cast<int>(l_exitCode);
		}

		::CloseHandle(l_processInfo.hProcess);
		::CloseHandle(l_processInfo.hThread);
		return l_result;
	}
#else
	ProcessResult runProcessTimed(const std::string& v_Command, unsigned /*v_TimeoutSeconds*/, const std::string& v_CaptureFilePath) {
		// No portable kill-on-timeout / output redirection without a Win32
		// handle to wait on - falls back to an untimed, uncaptured run on
		// non-Windows.
		(void)v_CaptureFilePath;
		ProcessResult l_result;
		l_result.m_ExitCode = runProcess(v_Command);
		return l_result;
	}
#endif

}
