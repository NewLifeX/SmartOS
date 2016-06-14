using System;
using System.Collections;
using System.Diagnostics;
using System.Reflection;
using System.Text;
using System.Linq;
using System.IO;
using System.Collections.Generic;
using Microsoft.Win32;
using NewLife.Log;
using System.Text.RegularExpressions;
using System.Threading;
using System.Xml;
using NewLife.Web;

namespace NewLife.Reflection
{
    public class ScriptEngine
    {
        static void Main()
        {
            var build = new Builder();
            build.Init();
			build.Cortex = 4;
			build.AddIncludes("..\\..\\..\\Lib\\CMSIS");
			build.AddIncludes("..\\..\\..\\Lib\\Inc");
			build.AddIncludes("..\\", false);
			build.AddIncludes("..\\..\\", false);
			build.AddIncludes("..\\..\\Core");
			build.AddIncludes("..\\..\\Kernel");
			build.AddIncludes("..\\..\\Device");
            build.AddFiles(".", "*.c;*.cpp;*.s");
            build.AddFiles("..\\CortexM", "*.c;*.cpp;*.s");
			build.Libs.Clear();
            build.CompileAll();
            build.BuildLib("..\\..\\");

			build.Debug = true;
            build.CompileAll();
            build.BuildLib("..\\..\\");

			/*build.Tiny = true;
            build.CompileAll();
            build.BuildLib("..\\");*/
        }
    }
}
	//include=..\..\Tool\MDK.cs
