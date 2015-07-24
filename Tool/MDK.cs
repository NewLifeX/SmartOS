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

namespace NewLife.Reflection
{
    public class Builder
    {
        #region 初始化编译器
        String Complier;
        String Asm;
        String Link;
        String Ar;
        String FromELF;
        String LibPath;

        public Boolean Init(String basePath = null)
        {
            if (basePath.IsNullOrEmpty())
            {
                var mdk = new MDK();
                if (mdk.ToolPath.IsNullOrEmpty() || !Directory.Exists(mdk.ToolPath)) return false;

                XTrace.WriteLine("发现 {0} {1} {2}", mdk.Name, mdk.Version, mdk.ToolPath);
                basePath = mdk.ToolPath.CombinePath("ARMCC\\bin").GetFullPath();
            }

            Complier = basePath.CombinePath("armcc.exe");
            Asm = basePath.CombinePath("armasm.exe");
            Link = basePath.CombinePath("armlink.exe");
            Ar = basePath.CombinePath("armar.exe");
            FromELF = basePath.CombinePath("fromelf.exe");
            LibPath = basePath.CombinePath("..\\..\\").GetFullPath();

            //if (LibPath.AsDirectory().Exists) Includes.Add(LibPath);

            // 扫描当前所有目录，作为头文件引用目录
            foreach (var item in ".".GetFullPath().AsDirectory().GetDirectories("*", SearchOption.AllDirectories))
            {
                Includes.Add(item.FullName);
            }

            return true;
        }
        #endregion

        #region 编译选项
        private Boolean _Debug;
        /// <summary>是否编译调试版</summary>
        public Boolean Debug { get { return _Debug; } set { _Debug = value; } }

        private String _CPU = "Cortex-M0";
        /// <summary>处理器</summary>
        public String CPU { get { return _CPU; } set { _CPU = value; } }

        private String _Flash = "STM32F0XX";
        /// <summary>Flash容量</summary>
        public String Flash { get { return _Flash; } set { _Flash = value; } }

        private Int32 _Cortex;
        /// <summary>Cortex版本</summary>
        public Int32 Cortex
        {
            get { return _Cortex; }
            set
            {
                _Cortex = value;
                CPU = "Cortex-M{0}".F(value);
                Flash = "STM32F{0}XX".F(value);
            }
        }

        private Boolean _GD32;
        /// <summary>是否GD32芯片</summary>
        public Boolean GD32 { get { return _GD32; } set { _GD32 = value; } }

        private List<String> _Defines = new List<String>();
        /// <summary>定义集合</summary>
        public List<String> Defines { get { return _Defines; } set { _Defines = value; } }

        private List<String> _Includes = new List<String>() { "..\\SmartOS", "..\\SmartOSLib", "..\\SmartOSLib\\inc" };
        /// <summary>引用头文件路径集合</summary>
        public List<String> Includes { get { return _Includes; } set { _Includes = value; } }

        private List<String> _Objs = new List<String>();
        /// <summary>对象文件集合</summary>
        public List<String> Objs { get { return _Objs; } set { _Objs = value; } }
        #endregion

        #region 主要编译方法
        public Int32 CompileCPP(String file)
        {
            /*
             * -c --cpu Cortex-M0 -D__MICROLIB -g -O3 --apcs=interwork --split_sections -I..\Lib\inc -I..\Lib\CMSIS -I..\SmartOS 
             * -DSTM32F030 -DUSE_STDPERIPH_DRIVER -DSTM32F0XX -DGD32 -o ".\Obj\*.o" --omf_browse ".\Obj\*.crf" --depend ".\Obj\*.d" 
             * 
             * -c --cpu Cortex-M3 -D__MICROLIB -g -O0 --apcs=interwork --split_sections -I..\STM32F1Lib\inc -I..\STM32F1Lib\CMSIS -I..\SmartOS 
             * -DSTM32F10X_HD -DDEBUG -DUSE_FULL_ASSERT -o ".\Obj\*.o" --omf_browse ".\Obj\*.crf" --depend ".\Obj\*.d" 

             */

            //var lstName = ".\\Lst\\" + Path.GetFileNameWithoutExtension(file);
            var objName = ".\\Obj\\" + Path.GetFileNameWithoutExtension(file);
            //lstName = lstName.GetFullPath();
            objName = objName.GetFullPath();

            // 如果文件太新，则不参与编译
            var obj = (objName + ".o").AsFile();
            if (obj.Exists && obj.LastWriteTime > file.AsFile().LastWriteTime) return 0;

            var sb = new StringBuilder();
            sb.AppendFormat("-c --cpu {0} -D__MICROLIB -g -O{1} --apcs=interwork --split_sections -DUSE_STDPERIPH_DRIVER", CPU, Debug ? 0 : 3);
            sb.AppendFormat(" -D{0}", Flash);
            if (GD32) sb.Append(" -DGD32");
            foreach (var item in Defines)
            {
                sb.AppendFormat(" -D{0}", item);
            }
            if (Debug) sb.Append(" -DDEBUG -DUSE_FULL_ASSERT");
            foreach (var item in Includes)
            {
                sb.AppendFormat(" -I{0}", item);
            }

            //sb.AppendFormat(" --feedback \"{0}.feedback\"", lstName);
            sb.AppendFormat(" -o \"{0}.o\" --omf_browse \"{0}.crf\" --depend \"{0}.d\"", objName);
            sb.AppendFormat(" -c \"{0}\"", file);

            return Complier.Run(sb.ToString(), 3000, WriteLog);
        }

        public Int32 Assemble(String file)
        {
            /*
             * --cpu Cortex-M3 -g --apcs=interwork --pd "__MICROLIB SETA 1" 
             * --pd "__UVISION_VERSION SETA 515" --pd "STM32F10X_HD SETA 1" --list ".\Lis\*.lst" --xref -o "*.o" --depend "*.d" 
             */

            var lstName = ".\\Lst\\" + Path.GetFileNameWithoutExtension(file);
            var objName = ".\\Obj\\" + Path.GetFileNameWithoutExtension(file);
            lstName = lstName.GetFullPath();
            objName = objName.GetFullPath();

            // 如果文件太新，则不参与编译
            var obj = (objName + ".o").AsFile();
            if (obj.Exists && obj.LastWriteTime > file.AsFile().LastWriteTime) return 0;

            var sb = new StringBuilder();
            sb.AppendFormat("--cpu {0} -g --apcs=interwork --pd \"__MICROLIB SETA 1\"", CPU);
            sb.AppendFormat(" --pd \"{0} SETA 1\"", Flash);

            sb.AppendFormat(" --list \"{0}.lst\" --xref -o \"{1}.o\" --depend \"{1}.d\"", lstName, objName);
            sb.AppendFormat(" \"{0}\"", file);

            return Asm.Run(sb.ToString(), 3000, WriteLog);
        }

        public Int32 CompileAll(String path, String exts = "*.c;*.cpp;*.s")
        {
            var count = 0;

            // 提前创建临时目录
            var obj = "Obj".GetFullPath().EnsureDirectory(false);
            "Lst".GetFullPath().EnsureDirectory(false);

            path = path.GetFullPath().EnsureEnd("\\");
            foreach (var item in path.AsDirectory().GetAllFiles(exts, true))
            {
                if (!item.Extension.EqualIgnoreCase(".c", ".cpp", ".s")) continue;

                //var file = item.FullName;
                //if (file.StartsWithIgnoreCase(path)) file = file.TrimStart(path);

                var rs = 0;
                Console.WriteLine("编译：{0}", item.FullName);
                var sw = new Stopwatch();
                sw.Start();
                switch (item.Extension.ToLower())
                {
                    case ".c":
                    case ".cpp":
                        rs = CompileCPP(item.FullName);
                        break;
                    case ".s":
                        rs = Assemble(item.FullName);
                        break;
                    default:
                        break;
                }

                sw.Stop();
                var old = Console.ForegroundColor;
                Console.ForegroundColor = ConsoleColor.Green;
                Console.WriteLine("\t {0:n0}毫秒", sw.ElapsedMilliseconds);
                Console.ForegroundColor = old;

                if (rs == 0)
                {
                    count++;

                    Objs.Add(obj.CombinePath(Path.GetFileNameWithoutExtension(item.FullName) + ".o"));
                }
            }

            return count;
        }

        public void BuildLib(String name = null)
        {
            if (name.IsNullOrEmpty()) name = ".".GetFullPath().AsDirectory().Name;

            var sb = new StringBuilder();
            sb.Append("--create -c");
            sb.AppendFormat(" -r \"{0}\"", name.EnsureEnd(".lib").GetFullPath());

            foreach (var item in Objs)
            {
                sb.Append(" ");
                sb.Append(item);
            }

            Ar.Run(sb.ToString(), 3000, WriteLog);
        }

        public Int32 Build(String name = null)
        {
            if (name.IsNullOrEmpty()) name = ".".GetFullPath().AsDirectory().Name;

            Console.WriteLine("链接：{0}", name);

            /*
             * --cpu Cortex-M3 *.o --library_type=microlib --strict --scatter ".\Obj\SmartOSF1_Debug.sct" 
             * --summary_stderr --info summarysizes --map --xref --callgraph --symbols 
             * --info sizes --info totals --info unused --info veneers 
             * --list ".\Lis\SmartOSF1_Debug.map" 
             * -o .\Obj\SmartOSF1_Debug.axf 
             * 
             * --cpu Cortex-M0 *.o --library_type=microlib --diag_suppress 6803 --strict --scatter ".\Obj\Smart130.sct" 
             * --summary_stderr --info summarysizes --map --xref --callgraph --symbols 
             * --info sizes --info totals --info unused --info veneers 
             * --list ".\Lis\Smart130.map" 
             * -o .\Obj\Smart130.axf 
             */

            var lstName = ".\\Lst\\" + Path.GetFileNameWithoutExtension(name);
            var objName = ".\\Obj\\" + Path.GetFileNameWithoutExtension(name);
            lstName = lstName.GetFullPath();
            objName = objName.GetFullPath();

            var sb = new StringBuilder();
            sb.AppendFormat("--cpu {0} --library_type=microlib --strict", CPU);
            //sb.AppendFormat(" --scatter \"{0}.sct\"", objName);
            sb.Append(" --summary_stderr --info summarysizes --map --xref --callgraph --symbols");
            sb.Append(" --info sizes --info totals --info unused --info veneers");

            sb.AppendFormat(" --list \"{0}.map\" -o \"{1}\"", lstName, name.EnsureEnd(".axf").GetFullPath());

            foreach (var item in Objs)
            {
                sb.Append(" ");
                sb.Append(item);
            }

            var rs = Link.Run(sb.ToString(), 3000, WriteLog);
            if (rs != 0) return rs;

            sb.Clear();
            sb.AppendFormat("--bin  -o \"{0}\" \"{1}\"", name.EnsureEnd(".bin").GetFullPath(), name.EnsureEnd(".axf").GetFullPath());
            rs = FromELF.Run(sb.ToString(), 3000, WriteLog);

            return rs;
        }
        #endregion

        #region 日志
        void WriteLog(String msg)
        {
            if (msg.IsNullOrEmpty()) return;

            msg = FixWord(msg);
            if (msg.StartsWithIgnoreCase("错误"))
                XTrace.Log.Error(msg);
            else
                XTrace.WriteLine(msg);
        }

        private Dictionary<String, String> _Words = new Dictionary<String, String>(StringComparer.OrdinalIgnoreCase);
        /// <summary>字典集合</summary>
        public Dictionary<String, String> Words { get { return _Words; } set { _Words = value; } }

        public String FixWord(String msg)
        {
            #region 初始化
            if (Words.Count == 0)
            {
                Words.Add("Error", "错误");
                Words.Add("Warning", "警告");
                Words.Add("Warnings", "警告");
                Words.Add("cannot", "不能");
                Words.Add("open", "打开");
                Words.Add("source", "源");
                Words.Add("input", "输入");
                Words.Add("file", "文件");
                Words.Add("No", "没有");
                Words.Add("Not", "没有");
                Words.Add("such", "该");
                Words.Add("or", "或");
                Words.Add("And", "与");
                Words.Add("Directory", "目录");
                Words.Add("Enough", "足够");
                Words.Add("Information", "信息");
                Words.Add("to", "去");
                Words.Add("list", "列出");
                Words.Add("image", "镜像");
                Words.Add("Symbols", "标识");
                Words.Add("the", "");
                Words.Add("map", "映射");
                Words.Add("Finished", "完成");
                Words.Add("line", "行");
                Words.Add("messages", "消息");
            }
            #endregion

            //var sb = new StringBuilder();
            //var ss = msg.Trim().Split(" ", ":", "(", ")");
            //var ss = msg.Trim().Split(" ");
            //for (int i = 0; i < ss.Length; i++)
            //{
            //    var rs = "";
            //    if (Words.TryGetValue(ss[i], out rs)) ss[i] = rs;
            //}
            //return String.Join(" ", ss);
            //var ms = Regex.Matches(msg, "");
            msg = Regex.Replace(msg, "(\\w+\\s?)", match =>
            {
                var w = match.Captures[0].Value;
                var rs = "";
                if (Words.TryGetValue(w.Trim(), out rs)) w = rs;
                return w;
            });
            return msg;
        }
        #endregion
    }

    /// <summary>MDK环境</summary>
    public class MDK
    {
        #region 属性
        private String _Name = "MDK";
        /// <summary>名称</summary>
        public String Name { get { return _Name; } set { _Name = value; } }

        private String _Version;
        /// <summary>版本</summary>
        public String Version { get { return _Version; } set { _Version = value; } }

        private String _ToolPath;
        /// <summary>工具目录</summary>
        public String ToolPath { get { return _ToolPath; } set { _ToolPath = value; } }
        #endregion

        #region 初始化
        /// <summary>初始化</summary>
        public MDK()
        {
            #region 从注册表获取目录和版本
            if (String.IsNullOrEmpty(ToolPath))
            {
                var reg = Registry.LocalMachine.OpenSubKey("Software\\Keil\\Products\\MDK");
                if (reg == null) reg = Registry.LocalMachine.OpenSubKey("Software\\Wow6432Node\\Keil\\Products\\MDK");
                if (reg != null)
                {
                    ToolPath = reg.GetValue("Path") + "";
                    if (String.IsNullOrEmpty(Version)) Version = (reg.GetValue("Version") + "").Trim('V', 'v', 'a', 'b', 'c');

                    //if (!String.IsNullOrEmpty(ToolPath)) XTrace.WriteLine("从注册表得到路径{0} {1}！", ToolPath, Version);
                }
            }
            #endregion

            #region 扫描所有根目录，获取MDK安装目录
            if (String.IsNullOrEmpty(ToolPath))
            {
                foreach (var item in DriveInfo.GetDrives())
                {
                    if (!item.IsReady) continue;

                    var p = Path.Combine(item.RootDirectory.FullName, "Keil\\ARM");
                    if (Directory.Exists(p))
                    {
                        ToolPath = p;
                        break;
                    }
                }
            }
            if (String.IsNullOrEmpty(ToolPath)) throw new Exception("无法获取MDK安装目录！");
            #endregion

            #region 自动获取版本
            if (String.IsNullOrEmpty(Version))
            {
                var p = Path.Combine(ToolPath, "..\\Tools.ini");
                if (File.Exists(p))
                {
                    foreach (var item in File.ReadAllLines(p))
                    {
                        if (String.IsNullOrEmpty(item)) continue;
                        if (item.StartsWith("VERSION=", StringComparison.OrdinalIgnoreCase))
                        {
                            Version = item.Substring("VERSION=".Length).Trim().Trim('V', 'v', 'a', 'b', 'c');
                            break;
                        }
                    }
                }
            }
            #endregion
        }
        #endregion
    }
}