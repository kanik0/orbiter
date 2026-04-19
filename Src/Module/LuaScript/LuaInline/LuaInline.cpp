// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ==============================================================
//           ORBITER MODULE: LUA Inline Interpreter
//                  Part of the ORBITER SDK
//            Copyright (C) 2007 Martin Schweiger
//                   All rights reserved
//
// LuaInline.cpp
// This library is loaded by the Orbiter core on demand to provide
// interpreter instances to modules via API requests.
//
// Notes:
// * LuaInline.dll must be placed in the Orbiter root directory
//   (not in the Modules subdirectory). It is loaded automatically
//   by Orbiter when required. It must not be loaded manually via
//   the Launchpad "Modules" tab.
// * LuaInline.dll depends on LuaInterpreter.dll and Lua5.1.dll
//   which must also be present in the Orbiter root directory.
// ==============================================================

#define STRICT
#define ORBITER_MODULE
#include "orbitersdk.h"
#include "LuaInline.h"
#ifdef _WIN32
#include <direct.h>
#endif
#include <chrono>
#include <future>

// ==============================================================
// class InterpreterList::Environment: implementation

InterpreterList::Environment::Environment()
{
	cmd = NULL;
	singleCmd = false;
	interp = CreateInterpreter ();
}

InterpreterList::Environment::~Environment()
{
	if (!interp) return;
	if (thread.joinable()) {
		// Signal the worker to exit its WaitExec and unwind. EndExec hands
		// the execution slot back so the thread can observe termInterp.
		termInterp = true;
		interp->Terminate();
		interp->EndExec();

		// std::thread has no timed join, so run the join on a helper and
		// wait up to 1 second — matches the old WaitForSingleObject(1000)
		// fallback. If the worker is wedged we detach rather than calling
		// TerminateThread (POSIX has no safe equivalent, and even on
		// Windows this path was a last resort). Detaching leaks the worker
		// to process exit; oapiWriteLog surfaces it so it isn't silent.
		auto joiner = std::async(std::launch::async,
			[this]{ thread.join(); });
		if (joiner.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
			oapiWriteLog((char*)"LuaInline: timeout while waiting for interpreter thread");
			thread.detach();
		}
	}
	delete interp;
}

Interpreter *InterpreterList::Environment::CreateInterpreter ()
{
	termInterp = false;
	interp = new Interpreter ();
	interp->Initialise();
	thread = std::thread(&InterpreterList::Environment::InterpreterThreadProc, this);
	return interp;
}

void InterpreterList::Environment::InterpreterThreadProc (InterpreterList::Environment *env)
{
	Interpreter *interp = env->interp;
	// interpreter loop
	for (;;) {
		interp->WaitExec(); // wait for execution permission
		if (env->termInterp) break; // close thread requested
		if (env->cmd) {
			interp->RunChunk (env->cmd, strlen (env->cmd)); // run command from buffer
			delete []env->cmd;
			env->cmd = 0;
			if (env->singleCmd) break;
		} else {
			interp->RunChunk ("", 0); // idle loop
		}
		if (interp->Status() == 1) break;
		interp->EndExec();  // return control
	}
	interp->EndExec();  // return execution slot
}


// ==============================================================
// class InterpreterList: implementation

InterpreterList::InterpreterList (HINSTANCE hDLL): Module (hDLL)
{
	nlist = nbuf = 0;
}

InterpreterList::~InterpreterList ()
{
	while (nlist) DelInterpreter(list[0]);
}

void InterpreterList::clbkSimulationStart (RenderMode mode)
{
}

void InterpreterList::clbkSimulationEnd ()
{
	while (nlist) DelInterpreter(list[0]);
}

void InterpreterList::clbkPostStep (double simt, double simdt, double mjd)
{
	DWORD i;
	for (i = 0; i < nlist; i++) // prune all finished interpreters
		if (!list[i]->interp) DelInterpreter (list[i--]);

	for (i = 0; i < nlist; i++) { // let the interpreter do some work
		if (list[i]->interp->IsBusy() || list[i]->cmd || list[i]->interp->nJobs()) {
			list[i]->interp->EndExec();
			list[i]->interp->WaitExec();
		}
	}
}

void InterpreterList::clbkDeleteVessel (OBJHANDLE hVessel)
{
	Interpreter::DeleteVessel(hVessel);
}

InterpreterList::Environment *InterpreterList::AddInterpreter ()
{
	if (nlist == nbuf) { // increase buffer size
		Environment **tmp = new Environment*[nbuf += 16];
		if (nlist) {
			memcpy (tmp, list, nlist*sizeof(Environment*));
			delete []list;
		}
		list = tmp;
	}

	Environment *env = new Environment;
	list[nlist++] = env;
	return env;
}

int InterpreterList::DelInterpreter (InterpreterList::Environment *env)
{
	// remove interpreter from list
	DWORD i, j;
	for (i = 0; i < nlist; i++)
		if (list[i] == env) break;
	if (i == nlist) return 2; // interpreter not found
	for (j = i+1; j < nlist; j++)
		list[j-1] = list[j];
	nlist--;

	// delete interpreter
	delete env;
	return 0;
}

// ==============================================================
// API interface

static InterpreterList *g_IList = nullptr;

DLLCLBK void InitModule (HINSTANCE hDLL)
{
	g_IList = new InterpreterList (hDLL);
	oapiRegisterModule (g_IList);
}

DLLCLBK void ExitModule (HINSTANCE hDLL)
{
	if (g_IList) {
		delete g_IList;
		g_IList = nullptr;
	}
}

// interpreter-specific callback functions
DLLCLBK INTERPRETERHANDLE opcNewInterpreter ()
{
	if (g_IList) {
		InterpreterList::Environment *env = g_IList->AddInterpreter();
		return (INTERPRETERHANDLE)env;
	} else {
		return NULL;
	}
}

DLLCLBK int opcDelInterpreter (INTERPRETERHANDLE hInterp)
{
	if (g_IList)
		return g_IList->DelInterpreter ((InterpreterList::Environment*)hInterp);
	else
		return 1;
}

DLLCLBK INTERPRETERHANDLE opcRunInterpreter (const char *cmd)
{
	if (g_IList) {
		InterpreterList::Environment *env = g_IList->AddInterpreter();
		env->cmd = new char[strlen(cmd)+10];
		sprintf (env->cmd, "run('%s')", cmd);
		return (INTERPRETERHANDLE)env;
	} else {
		return NULL;
	}
}

DLLCLBK bool opcAsyncScriptCmd (INTERPRETERHANDLE hInterp, const char *cmd)
{
	InterpreterList::Environment *env = (InterpreterList::Environment*)hInterp;
	char *str;
	if (env->cmd) { // command still waiting: append new command
		str = new char[strlen(env->cmd)+strlen(cmd)+2];
		strcpy(str,env->cmd);
		strcat(str,";");
		strcat(str,cmd);
		char *tmp = env->cmd;
		env->cmd = str;
		delete []tmp;
	} else {
		str = new char[strlen(cmd)+1];
		strcpy (str, cmd);
		env->cmd = str;
	}
	return true;
}

DLLCLBK bool opcExecScriptCmd (INTERPRETERHANDLE hInterp, const char *cmd)
{
	InterpreterList::Environment *env = (InterpreterList::Environment*)hInterp;
	char *str = new char[strlen(cmd)+1];
	char *cmd_async = 0;
	strcpy (str, cmd);
	if (env->cmd) // asynchronous request is waiting
		cmd_async = env->cmd;
	env->cmd = str;
	while (env->cmd) {
		// wait until command has been executed
		env->interp->EndExec();
		env->interp->WaitExec();
	}
	if (cmd_async) // restore the asynchronous request
		env->cmd = cmd_async;
	return true;
}

DLLCLBK lua_State *opcGetLua (INTERPRETERHANDLE hInterp)
{
	InterpreterList::Environment *env = (InterpreterList::Environment*)hInterp;
	return env->interp->GetState();
}
