# HCOM4DB

Hyper Communication Library for Database (HCOM4DB) is a high-performance and high-availability communication component. It is developed based on Huawei HCOM.
It is a function module on which components such as the CBB depend when using communication protocols such as RDMA.

#### 1. Project Description
1. Programming language: C/C++

2. Compilation tool: CMake

3. Directories:

`hcom4db`: the main directory. The `CMakeLists.txt` file is the main project entry.

`src`: the source code directory

`build`: the project building script directory
#### 2. Compilation Guide
1. OS and Software Dependencies

The following OSs are supported:

CentOS 7.6 (x86)

openEuler 20.03 LTS

openEuler 22.03 LTS

openEuler 24.03 LTS

For details about how to adapt to other OSs, see the openGauss compilation guide.

2. Downloading HCOM4DB

Download HCOM4DB from the openGauss open-source community.

3. Compiling Code

Use `hcom4db/build/linux/opengauss/build.sh` to compile the code. The following table describes the parameters.
<table>
    <tr>
        <th>Option</th>
        <th>Parameter</th>
        <th>Description</th>
    </tr>
    <tr>
        <th>-3rd</th>
        <th>[binarylibs path]</th>
        <th>Specifies the `binarylibs` path, which must be an absolute path.</th>
    </tr>
    <tr>
        <th>-m</th>
        <th>[version_mode]</th>
        <th>Specifies the target version to be compiled, which can be `Debug` or `Release` (default).</th>.
    </tr>
    <tr>
        <th>-t</th>
        <th>[build_tool]</th>
        <th>Specifies the compilation tool, which defaults to `cmake`.</th>
    </tr>
</table>

Run the following command to perform compilation:

`[user@linux ]$ sh build.sh -3rd [binarylibs path] -m Release -t cmake`

After the compilation is complete, the dynamic library is generated under the `hcom4db/output/lib` directory.
