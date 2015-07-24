using System;
using System.Collections;
using System.Diagnostics;
using System.Reflection;
using System.Text;
using System.IO;
using NewLife.Log;

namespace NewLife.Reflection
{
    public class ScriptEngine
    {
        /// <summary>总文件数</summary>
        static Int32 Total;
        /// <summary>已被处理文件数</summary>
        static Int32 Fixed;

        //static void Main(params String[] args)
        static void Main()
        {
            //if (args.Length > 0 && !String.IsNullOrEmpty(args[0]) && Directory.Exists(args[0]))
            //    PathHelper.BaseDirectory = args[0];
            //else 
            if (Directory.Exists(Environment.CurrentDirectory))
                PathHelper.BaseDirectory = Environment.CurrentDirectory;

            Total = 0;
            Fixed = 0;
            try
            {
                ProcessFileType("*.h");
                ProcessFileType("*.c");
                ProcessFileType("*.hpp");
                ProcessFileType("*.cpp");
                ProcessFileType("*.s");
                ProcessFileType("*.asm");
                ProcessFileType("*.cs");

                Console.WriteLine("共发现文件{0}个，处理{1}个！", Total, Fixed);
            }
            catch (Exception ex)
            {
                XTrace.WriteException(ex);
            }
        }

        /// <summary>处理指定类型的文件</summary>
        /// <param name="ext"></param>
        static void ProcessFileType(String ext)
        {
            var root = ".".GetFullPath().EnsureEnd("\\");
            foreach (var fi in Directory.GetFiles(root, ext, SearchOption.AllDirectories))
            {
                Total++;
                Console.Write(fi.TrimStart(root));

                var flag = false;
                var encoding = Encoding.Default;
                var txt = "";

                // 特殊打开，为了获取文件编码
                using (var reader = new StreamReader(fi, encoding, true))
                {
                    encoding = reader.CurrentEncoding;
                    txt = reader.ReadToEnd();
                }

                //Console.Write(" 文件编码：{0}", encoding.WebName);
                if (encoding != Encoding.UTF8)
                {
                    // 取头3个字节检查是否带有编码头
                    var buf = File.ReadAllBytes(fi).ReadBytes(3);
                    var buf2 = encoding.GetBytes(txt.Substring(0, 3));
                    // 如果没有校验头，则默认用UTF8编码
                    if (buf.CompareTo(buf2) == 0) encoding = Encoding.UTF8;
                }

                flag |= CutHeaderComment(ref txt);
                flag |= CutFooterComment(ref txt);

                // 如果修改过，按照原编码写入
                if (flag)
                {
                    File.WriteAllText(fi, txt, encoding);
                    Fixed++;
                }

                Console.WriteLine();
            }
        }

        /// <summary>削掉头部注释</summary>
        /// <param name="content"></param>
        /// <returns></returns>
        static Boolean CutHeaderComment(ref String content)
        {
            if (String.IsNullOrEmpty(content)) return false;

            var txt = content;
            // 循环状态
            var flag = false;
            // 循环处理
            do
            {
                flag = false;

                // 干掉前导空白
                txt = txt.TrimStart();

                // 干掉单行注释
                if (txt.StartsWith("//"))
                {
                    flag = true;

                    var p = txt.IndexOf(Environment.NewLine);
                    // 如果找不到行尾，认为全部都是注释，立马返回
                    if (p < 0)
                    {
                        content = null;
                        return true;
                    }

                    txt = txt.Substring(p + Environment.NewLine.Length);
                }

                // 干掉成片注释
                if (txt.StartsWith("/*"))
                {
                    flag = true;

                    var p = txt.IndexOf("*/");
                    // 如果找不到行尾，认为全部都是注释，立马返回
                    if (p < 0)
                    {
                        content = null;
                        return true;
                    }

                    txt = txt.Substring(p + Environment.NewLine.Length);
                }

                // 汇编的注释是分号开头
                if (txt.StartsWith(";"))
                {
                    flag = true;

                    var p = txt.IndexOf(Environment.NewLine);
                    // 如果找不到行尾，认为全部都是注释，立马返回
                    if (p < 0)
                    {
                        content = null;
                        return true;
                    }

                    txt = txt.Substring(p + Environment.NewLine.Length);
                }
            } while (flag && !String.IsNullOrEmpty(txt));

            if (txt == content) return false;

            content = txt;

            return true;
        }

        static Boolean CutFooterComment(ref String content)
        {
            if (String.IsNullOrEmpty(content)) return false;

            var txt = content;
            // 循环状态
            var flag = false;
            // 循环处理
            do
            {
                flag = false;

                // 干掉结尾空白
                txt = txt.TrimEnd();

                // 干掉单行注释
                var lastp = txt.LastIndexOf(Environment.NewLine);
                if (lastp >= 0)
                {
                    var line = txt.Substring(lastp + Environment.NewLine.Length).TrimStart();
                    if (line.StartsWith("//"))
                    {
                        flag = true;

                        txt = txt.Substring(0, lastp);
                    }
                    // 汇编的注释是分号开头
                    if (line.StartsWith(";"))
                    {
                        flag = true;

                        txt = txt.Substring(0, lastp);
                    }
                }

                // 干掉成片注释
                if (txt.EndsWith("*/"))
                {
                    flag = true;

                    var p = txt.LastIndexOf("/*");
                    // 如果找不到，认为全部都是注释，立马返回
                    if (p < 0)
                    {
                        content = null;
                        return true;
                    }

                    txt = txt.Substring(0, p);
                }
            } while (flag && !String.IsNullOrEmpty(txt));

            // 要求最后有一行空行
            txt += Environment.NewLine;
            if (txt == content) return false;

            content = txt;

            return true;
        }
    }
}
