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
			if(Directory.Exists("ObjD"))Directory.Delete("ObjD",true);
			if(Directory.Exists("Obj"))Directory.Delete("Obj",true);
			if(Directory.Exists("List"))Directory.Delete("List",true);
        }
    }
}
