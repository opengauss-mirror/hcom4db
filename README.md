# HCOM4DB

HCOM4DB(Hyper Communication Library for Database)是基于华为公司开发HCOM通信组件，面向数据库场景开发的高性能、高可用的通信组件。
CBB等组件使用RDMA等通信协议依赖的函数模块。

#### 一、工程说明
1、编程语言：C/C++

2、编译工具：cmake

3、目录说明：

hcom4db：主目录，CMakeLists.txt为主工程入口；

src: 源代码目录；

build：工程构建脚本目录。
#### 二、编译指导
1、操作系统和软件依赖要求

支持以下操作系统：

CentOS 7.6（x86）

openEuler-20.03-LTS

openEuler-22.03-LTS

openEuler-24.03-LTS

适配其他系统，可参照openGauss数据库编译指导

2、下载HCOM4DB

可以从openGauss开源社区下载HCOM4DB。

3、代码编译

使用hcom4db/build/linux/opengauss/build.sh编译代码, 参数说明请见以下表格。
<table>
    <tr>
        <th>选项</th>
        <th>参数</th>
        <th>说明</th>
    </tr>
    <tr>
        <th>-3rd</th>
        <th>[binarylibs path]</th>
        <th>指定binarylibs路径。该路径必须是绝对路径</th>
    </tr>
    <tr>
        <th>-m</th>
        <th>[version_mode]</th>
        <th>编译目标版本，Debug或者Release。默认Release</th>
    </tr>
    <tr>
        <th>-t</th>
        <th>[build_tool]</th>
        <th>指定编译工具，默认cmake</th>
    </tr>
</table>

现在只需使用如下命令即可编译：

[user@linux]$ sh build.sh -3rd [binarylibs path] -m Release -t cmake

完成编译后，动态库生成在hcom4db/output/lib目录中