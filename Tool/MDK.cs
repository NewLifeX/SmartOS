using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;
using Microsoft.Win32;
using NewLife.Log;

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
                if (mdk.ToolPath.IsNullOrEmpty() || !Directory.Exists(mdk.ToolPath))
                {
                    XTrace.WriteLine("未找到MDK！");
                    return false;
                }

                XTrace.WriteLine("发现 {0} {1} {2}", mdk.Name, mdk.Version, mdk.ToolPath);
                basePath = mdk.ToolPath.CombinePath("ARMCC\\bin").GetFullPath();
            }

            Complier = basePath.CombinePath("armcc.exe");
            Asm = basePath.CombinePath("armasm.exe");
            Link = basePath.CombinePath("armlink.exe");
            Ar = basePath.CombinePath("armar.exe");
            FromELF = basePath.CombinePath("fromelf.exe");
            LibPath = basePath.CombinePath("..\\..\\").GetFullPath();

            _Libs.Clear();
            Objs.Clear();

            // 扫描当前所有目录，作为头文件引用目录
            var ss = new String[] { ".", "..\\SmartOS" };
            foreach (var src in ss)
            {
                var p = src.GetFullPath();
				if (!Directory.Exists(p)) p = ("..\\" + src).GetFullPath();
				if (!Directory.Exists(p)) continue;

                AddIncludes(p, false);
                AddLibs(p);
            }
            ss = new String[] { "..\\Lib", "..\\SmartOSLib", "..\\SmartOSLib\\inc" };
            foreach (var src in ss)
            {
                var p = src.GetFullPath();
				if (!Directory.Exists(p)) p = ("..\\" + src).GetFullPath();
				if (!Directory.Exists(p)) continue;

                AddIncludes(p, true);
                AddLibs(p);
            }

            return true;
        }
        #endregion

        #region 编译选项
        private Boolean _Debug = true;
        /// <summary>是否编译调试版。默认true</summary>
        public Boolean Debug { get { return _Debug; } set { _Debug = value; } }

        private Boolean _Preprocess = false;
        /// <summary>是否仅预处理文件，不编译。默认false</summary>
        public Boolean Preprocess { get { return _Preprocess; } set { _Preprocess = value; } }

        private String _CPU = "Cortex-M0";
        /// <summary>处理器。默认M0</summary>
        public String CPU { get { return _CPU; } set { _CPU = value; } }

        private String _Flash = "STM32F0XX";
        /// <summary>Flash容量</summary>
        public String Flash { get { return _Flash; } set { _Flash = value; } }

        private Int32 _Cortex;
        /// <summary>Cortex版本。默认0</summary>
        public Int32 Cortex
        {
            get { return _Cortex; }
            set
            {
                _Cortex = value;
                CPU = "Cortex-M{0}".F(value);
                if (value == 3)
                    Flash = "STM32F10X";
                else
                    Flash = "STM32F{0}XX".F(value);
				if (value == 4) CPU += ".fp";
            }
        }

        private Boolean _GD32;
        /// <summary>是否GD32芯片</summary>
        public Boolean GD32 { get { return _GD32; } set { _GD32 = value; } }

        private ICollection<String> _Defines = new HashSet<String>(StringComparer.OrdinalIgnoreCase);
        /// <summary>定义集合</summary>
        public ICollection<String> Defines { get { return _Defines; } set { _Defines = value; } }

        private ICollection<String> _Includes = new HashSet<String>(StringComparer.OrdinalIgnoreCase);
        /// <summary>引用头文件路径集合</summary>
        public ICollection<String> Includes { get { return _Includes; } set { _Includes = value; } }

        private ICollection<String> _Files = new HashSet<String>(StringComparer.OrdinalIgnoreCase);
        /// <summary>源文件集合</summary>
        public ICollection<String> Files { get { return _Files; } set { _Files = value; } }

        private ICollection<String> _Objs = new HashSet<String>(StringComparer.OrdinalIgnoreCase);
        /// <summary>对象文件集合</summary>
        public ICollection<String> Objs { get { return _Objs; } set { _Objs = value; } }

        private IDictionary<String, String> _Libs = new Dictionary<String, String>(StringComparer.OrdinalIgnoreCase);
        /// <summary>库文件集合</summary>
        public IDictionary<String, String> Libs { get { return _Libs; } }
        #endregion

        #region 主要编译方法
        public Int32 Compile(String file)
        {
            /*
             * -c --cpu Cortex-M0 -D__MICROLIB -g -O3 --apcs=interwork --split_sections -I..\Lib\inc -I..\Lib\CMSIS -I..\SmartOS 
             * -DSTM32F030 -DUSE_STDPERIPH_DRIVER -DSTM32F0XX -DGD32 -o ".\Obj\*.o" --omf_browse ".\Obj\*.crf" --depend ".\Obj\*.d" 
             * 
             * -c --cpu Cortex-M3 -D__MICROLIB -g -O0 --apcs=interwork --split_sections -I..\STM32F1Lib\inc -I..\STM32F1Lib\CMSIS -I..\SmartOS 
             * -DSTM32F10X_HD -DDEBUG -DUSE_FULL_ASSERT -o ".\Obj\*.o" --omf_browse ".\Obj\*.crf" --depend ".\Obj\*.d" 

             */

            var objName = "Obj";
            if (Debug) objName += "D";
            objName = objName + "\\" + Path.GetFileNameWithoutExtension(file);

            // 如果文件太新，则不参与编译
            var obj = (objName + ".o").AsFile();
            if (obj.Exists && obj.LastWriteTime > file.AsFile().LastWriteTime) return 0;

            var sb = new StringBuilder();
            sb.AppendFormat("-c --cpp --cpu {0} -D__MICROLIB -g -O{1} --apcs=interwork --split_sections -DUSE_STDPERIPH_DRIVER", CPU, Debug ? 0 : 3);
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

			/*// 中文目录不要输出临时文件，MDK不支持
			if(Encoding.ASCII.GetByteCount(objName) == Encoding.UTF8.GetByteCount(objName))
			{
				sb.AppendFormat(" -o \"{0}.o\" --omf_browse \"{0}.crf\"", objName);
				sb.AppendFormat(" --depend \"{0}.d\"", objName);
			}*/
			if(Preprocess)
			{
				sb.AppendFormat(" -E");
				sb.AppendFormat(" -o \"{0}.{1}\" --omf_browse \"{0}.crf\" --depend \"{0}.d\"", objName, Path.GetExtension(file));
			}
			else
				sb.AppendFormat(" -o \"{0}.o\" --omf_browse \"{0}.crf\" --depend \"{0}.d\"", objName);
            sb.AppendFormat(" -c \"{0}\"", file);

            return Complier.Run(sb.ToString(), 100, WriteLog);
        }

        public Int32 Assemble(String file)
        {
            /*
             * --cpu Cortex-M3 -g --apcs=interwork --pd "__MICROLIB SETA 1" 
             * --pd "__UVISION_VERSION SETA 515" --pd "STM32F10X_HD SETA 1" --list ".\Lis\*.lst" --xref -o "*.o" --depend "*.d" 
             */

            var lstName = "Lst\\" + Path.GetFileNameWithoutExtension(file);
            var objName = "Obj";
            if (Debug) objName += "D";
            objName = objName + "\\" + Path.GetFileNameWithoutExtension(file);

            // 如果文件太新，则不参与编译
            var obj = (objName + ".o").AsFile();
            if (obj.Exists && obj.LastWriteTime > file.AsFile().LastWriteTime) return 0;

            var sb = new StringBuilder();
            sb.AppendFormat("--cpu {0} -g --apcs=interwork --pd \"__MICROLIB SETA 1\"", CPU);
            sb.AppendFormat(" --pd \"{0} SETA 1\"", Flash);

            sb.AppendFormat(" --list \"{0}.lst\" --xref -o \"{1}.o\" --depend \"{1}.d\"", lstName, objName);
            sb.AppendFormat(" \"{0}\"", file);

            return Asm.Run(sb.ToString(), 100, WriteLog);
        }

        public Int32 CompileAll()
        {
			Objs.Clear();
            var count = 0;

            // 提前创建临时目录
            var obj = "Obj";
            if (Debug) obj += "D";
            obj.GetFullPath().EnsureDirectory(false);
            "Lst".GetFullPath().EnsureDirectory(false);

            foreach (var item in Files)
            {
                Console.Write("编译：{0}\t", item);

                var rs = 0;
                var sw = new Stopwatch();
                sw.Start();
                switch (Path.GetExtension(item).ToLower())
                {
                    case ".c":
                    case ".cpp":
                        rs = Compile(item);
                        break;
                    case ".s":
                        rs = Assemble(item);
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

                    if(!Preprocess) Objs.Add(obj.CombinePath(Path.GetFileNameWithoutExtension(item) + ".o"));
                }
            }

            return count;
        }

        /// <summary>编译静态库</summary>
        /// <param name="name"></param>
        public void BuildLib(String name = null)
        {
            if (name.IsNullOrEmpty())
            {
                var file = Environment.GetEnvironmentVariable("XScriptFile");
                if (!file.IsNullOrEmpty())
                {
                    file = Path.GetFileNameWithoutExtension(file);
                    name = file.TrimStart("Build", "编译", "_").TrimEnd(".cs");
                }
            }
            if (name.IsNullOrEmpty()) name = ".".GetFullPath().AsDirectory().Name;
            if (Debug) name = name.EnsureEnd("D");

            XTrace.WriteLine("链接：{0}", name);

            var sb = new StringBuilder();
            sb.Append("--create -c");
            sb.AppendFormat(" -r \"{0}\"", name.EnsureEnd(".lib"));

            foreach (var item in Objs)
            {
                sb.Append(" ");
                sb.Append(item);
            }

            Ar.Run(sb.ToString(), 3000, WriteLog);
        }

        /// <summary>编译目标文件</summary>
        /// <param name="name"></param>
        /// <returns></returns>
        public Int32 Build(String name = null)
        {
            if (name.IsNullOrEmpty())
            {
                var file = Environment.GetEnvironmentVariable("XScriptFile");
                if (!file.IsNullOrEmpty())
                {
                    file = Path.GetFileNameWithoutExtension(file);
                    name = file.TrimStart("Build", "编译", "_").TrimEnd(".cs");
                }
            }
            if (name.IsNullOrEmpty()) name = ".".GetFullPath().AsDirectory().Name;
            name = Path.GetFileNameWithoutExtension(name);
            if (Debug) name = name.EnsureEnd("D");

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

            var lstName = "Lst\\" + name;
            lstName.EnsureDirectory();
            var objName = "Obj";
            if (Debug) objName += "D";
            objName = objName.CombinePath(name);

            var sb = new StringBuilder();
            sb.AppendFormat("--cpu {0} --library_type=microlib --strict", CPU);
            sb.AppendFormat(" --ro-base 0x08000000 --rw-base 0x20000000 --first __Vectors");
            //sb.AppendFormat(" --scatter \"{0}.sct\"", name.TrimEnd("D"));
            //sb.Append(" --summary_stderr --info summarysizes --map --xref --callgraph --symbols");
            //sb.Append(" --info sizes --info totals --info unused --info veneers");
            sb.Append(" --summary_stderr --info summarysizes --map --xref --callgraph --symbols");
            sb.Append(" --info sizes --info totals --info veneers");

            var axf = objName.EnsureEnd(".axf");
            sb.AppendFormat(" --list \"{0}.map\" -o \"{1}\"", lstName, axf);

            foreach (var item in Objs)
            {
                sb.Append(" ");
                sb.Append(item);
            }

            foreach (var item in Libs)
            {
				var d = item.Key.EndsWithIgnoreCase("D");
				if(Debug == d)
				{
					sb.Append(" ");
					sb.Append(item.Value);
				}
            }

            XTrace.WriteLine("链接：{0}", axf);

            var rs = Link.Run(sb.ToString(), 3000, WriteLog);
            if (rs != 0) return rs;

            var bin = name.EnsureEnd(".bin");
            XTrace.WriteLine("生成：{0}", bin);

            sb.Clear();
            sb.AppendFormat("--bin  -o \"{0}\" \"{1}\"", bin, axf);
            rs = FromELF.Run(sb.ToString(), 3000, WriteLog);

            return rs;
        }
        #endregion

        #region 辅助方法
		/// <summary>添加指定目录所有文件</summary>
        /// <param name="path">要编译的目录</param>
        /// <param name="exts">后缀过滤</param>
        /// <param name="excludes">要排除的文件</param>
        /// <returns></returns>
        public Int32 AddFiles(String path, String exts = "*.c;*.cpp", Boolean allSub = true, String excludes = null)
		{
            // 目标目录也加入头文件搜索
            AddIncludes(path);

            var count = 0;

            var excs = new HashSet<String>((excludes + "").Split(",", ";"), StringComparer.OrdinalIgnoreCase);

            path = path.GetFullPath().EnsureEnd("\\");
            if (String.IsNullOrEmpty(exts)) exts = "*.c;*.cpp";
            foreach (var item in path.AsDirectory().GetAllFiles(exts, allSub))
            {
                if (!item.Extension.EqualIgnoreCase(".c", ".cpp", ".s")) continue;

                Console.Write("添加：{0}\t", item.FullName);

                var flag = true;
				var ex = "";
                if (excs.Contains(item.Name)) { flag = false; ex = item.Name; }
				if(flag)
				{
					foreach (var elm in excs)
					{
						if (item.Name.Contains(elm)) { flag = false; ex = elm; break; }
					}
				}
                if (!flag)
				{
					var old2 = Console.ForegroundColor;
					Console.ForegroundColor = ConsoleColor.Yellow;
					Console.WriteLine("\t 跳过 {0}", ex);
					Console.ForegroundColor = old2;

					continue;
				}
				Console.WriteLine();

                if(!Files.Contains(item.FullName))
				{
					count++;
					Files.Add(item.FullName);
				}
            }
			
			return count;
		}
		
        public void AddIncludes(String path, Boolean sub = true, Boolean allSub = true)
        {
            path = path.GetFullPath();
            if (!Directory.Exists(path)) return;

            if (!Includes.Contains(path) && HasHeaderFile(path))
            {
                WriteLog("引用目录：{0}".F(path));
                Includes.Add(path);
            }

			if(sub)
			{
				var opt = allSub ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;
				foreach (var item in path.AsDirectory().GetDirectories("*", opt))
				{
					if (item.FullName.Contains(".svn")) continue;
					if (item.Name.EqualIgnoreCase("Lst", "Obj", "Log")) continue;

					if (!Includes.Contains(item.FullName) && HasHeaderFile(item.FullName))
					{
						WriteLog("引用目录：{0}".F(item.FullName));
						Includes.Add(item.FullName);
					}
				}
			}
        }

        Boolean HasHeaderFile(String path)
        {
            return path.AsDirectory().GetFiles("*.h", SearchOption.AllDirectories).Length > 0;
        }

        public void AddLibs(String path, Boolean allSub = true)
        {
            path = path.GetFullPath();
            if (!Directory.Exists(path)) return;

            var opt = allSub ? SearchOption.AllDirectories : SearchOption.TopDirectoryOnly;
            foreach (var item in path.AsDirectory().GetFiles("*.lib", opt))
            {
                var lib = new LibFile(item.FullName);
                // 调试版/发行版 优先选用最佳匹配版本
                var old = "";
                // 不包含，直接增加
                if (!_Libs.TryGetValue(lib.Name, out old))
                {
                    WriteLog("发现静态库：{0, -12} {1}".F(lib.Name, lib.FullName));
                    _Libs.Add(lib.Name, lib.FullName);
                }
                // 已包含，并且新版本更合适，替换
                else //if (lib.Debug == Debug)
                {
                    var lib2 = new LibFile(old);
                    if (lib2.Debug != Debug)
                    {
                        _Libs[lib.Name] = lib.FullName;
                        WriteLog("替换静态库：{0, -12} {1}".F(lib.Name, lib.FullName));
                    }
                }
            }
        }

        class LibFile
        {
            private String _Name;
            /// <summary>名称</summary>
            public String Name { get { return _Name; } set { _Name = value; } }

            private String _FullName;
            /// <summary>全名</summary>
            public String FullName { get { return _FullName; } set { _FullName = value; } }

            private Boolean _Debug;
            /// <summary>是否调试版文件</summary>
            public Boolean Debug { get { return _Debug; } set { _Debug = value; } }

            public LibFile(String file)
            {
                FullName = file;
                Name = Path.GetFileNameWithoutExtension(file);
                Debug = Name.EndsWithIgnoreCase("D");
            }
        }
        #endregion

        #region 日志
        void WriteLog(String msg)
        {
            if (msg.IsNullOrEmpty()) return;

            //msg = FixWord(msg);
            if (msg.StartsWithIgnoreCase("错误", "Error") || msg.Contains("Error:"))
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
                /*Words.Add("cannot", "不能");
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
                Words.Add("from", "自");
                Words.Add("list", "列出");
                Words.Add("image", "镜像");
                Words.Add("Symbol", "标识");
                Words.Add("Symbols", "标识");
                Words.Add("the", "");
                Words.Add("map", "映射");
                Words.Add("Finished", "完成");
                Words.Add("line", "行");
                Words.Add("messages", "消息");
                Words.Add("this", "这个");
                Words.Add("feature", "功能");
                Words.Add("supported", "被支持");
                Words.Add("on", "在");
                Words.Add("target", "目标");
                Words.Add("architecture", "架构");
                Words.Add("processor", "处理器");
                Words.Add("Undefined", "未定义");
                Words.Add("referred", "引用");*/
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