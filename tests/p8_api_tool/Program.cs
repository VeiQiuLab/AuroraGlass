using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;

static string N(Type t)
{
    if (t.IsByRef) return N(t.GetElementType()!) + "&";
    if (t.IsArray) return N(t.GetElementType()!) + "[]";
    if (!t.IsGenericType) return t.FullName ?? t.Name;
    string n=t.GetGenericTypeDefinition().FullName ?? t.Name;
    int p=n.IndexOf((char)96);
    if(p>=0)n=n[..p];
    return n+"<"+string.Join(",",t.GetGenericArguments().Select(N))+">";
}

static string P(ParameterInfo[] p)=>string.Join(",",p.Select(x=>N(x.ParameterType)));

if(args.Length!=5)return 2;

string mode=args[0];
string dll=Path.GetFullPath(args[1]);
string manifest=Path.GetFullPath(args[2]);
string version=args[3];
string prefix=args[4];
string dir=Path.GetDirectoryName(dll)!;

AssemblyLoadContext.Default.Resolving += (_, name) =>
{
    string fileName = name.Name + ".dll";

    foreach (string root in new[] { dir, AppContext.BaseDirectory })
    {
        string direct = Path.Combine(root, fileName);

        if (File.Exists(direct))
            return AssemblyLoadContext.Default.LoadFromAssemblyPath(direct);

        string? recursive = Directory
            .EnumerateFiles(root, fileName, SearchOption.AllDirectories)
            .FirstOrDefault();

        if (recursive is not null)
            return AssemblyLoadContext.Default.LoadFromAssemblyPath(recursive);
    }

    return null;
};

Assembly a=AssemblyLoadContext.Default.LoadFromAssemblyPath(dll);
string iv=a.GetCustomAttribute<AssemblyInformationalVersionAttribute>()?.InformationalVersion ?? "";
if(iv!=version)return 3;

Type[] types=a.GetExportedTypes().Where(t=>!t.IsDefined(typeof(CompilerGeneratedAttribute),false)).ToArray();
if(types.Any(t=>!(t.Namespace ?? "").StartsWith(prefix,StringComparison.Ordinal)))return 4;

var lines=new List<string>{"ASSEMBLY_VERSION|"+iv};

foreach(Type t in types.OrderBy(t=>t.FullName,StringComparer.Ordinal))
{
    string tn=N(t);
    lines.Add("TYPE|"+tn);

    foreach(var c in t.GetConstructors(BindingFlags.Public|BindingFlags.Instance|BindingFlags.DeclaredOnly).OrderBy(c=>P(c.GetParameters()),StringComparer.Ordinal))
        lines.Add("CTOR|"+tn+"|("+P(c.GetParameters())+")");

    foreach(var m in t.GetMethods(BindingFlags.Public|BindingFlags.Instance|BindingFlags.Static|BindingFlags.DeclaredOnly).Where(m=>!m.IsSpecialName).OrderBy(m=>m.Name,StringComparer.Ordinal).ThenBy(m=>P(m.GetParameters()),StringComparer.Ordinal))
        lines.Add("METHOD|"+tn+"|"+m.Name+"("+P(m.GetParameters())+")->"+N(m.ReturnType));

    foreach(var p in t.GetProperties(BindingFlags.Public|BindingFlags.Instance|BindingFlags.Static|BindingFlags.DeclaredOnly).OrderBy(p=>p.Name,StringComparer.Ordinal))
        lines.Add("PROPERTY|"+tn+"|"+p.Name+":"+N(p.PropertyType));

    foreach(var e in t.GetEvents(BindingFlags.Public|BindingFlags.Instance|BindingFlags.Static|BindingFlags.DeclaredOnly).OrderBy(e=>e.Name,StringComparer.Ordinal))
        lines.Add("EVENT|"+tn+"|"+e.Name+":"+N(e.EventHandlerType!));
}

// ASSEMBLY_VERSION is version metadata (tracks VERSION), not API shape.
// Exclude it from the baseline diff so the api/ baseline is version-robust.
string[] actual=lines.Where(x=>!x.StartsWith("ASSEMBLY_VERSION|",StringComparison.Ordinal)).Distinct(StringComparer.Ordinal).OrderBy(x=>x,StringComparer.Ordinal).ToArray();

if(mode=="snapshot")
{
    Directory.CreateDirectory(Path.GetDirectoryName(manifest)!);
    File.WriteAllLines(manifest,actual);
    Console.WriteLine("SNAPSHOT="+actual.Length);
    return 0;
}

string[] expected=File.ReadAllLines(manifest).Where(x=>x.Length>0 && !x.StartsWith("ASSEMBLY_VERSION|",StringComparison.Ordinal)).OrderBy(x=>x,StringComparer.Ordinal).ToArray();

if(!actual.SequenceEqual(expected,StringComparer.Ordinal))
{
    foreach(string x in expected.Except(actual,StringComparer.Ordinal))Console.Error.WriteLine("MISSING="+x);
    foreach(string x in actual.Except(expected,StringComparer.Ordinal))Console.Error.WriteLine("ADDED="+x);
    return 5;
}

Console.WriteLine("PUBLIC_API=PASS");
return 0;
