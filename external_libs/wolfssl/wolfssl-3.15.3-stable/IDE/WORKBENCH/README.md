## Wind River Workbench using VxWorks with wolfSSL
#### 1 Steps to Add wolfSSL to Workbench Project
1. Start by creating a new VxWorks image in Workbench by going to File > New >
Project and then selecting VxWorks Image Project.

2. Right click on the project and go to Import > General > Filesystem. Click Next.
Choose the path to the wolfSSL library here. Uncheck everything except the examples,
src and wolfcrypt directories. Uncheck the following:

        wolfcrypt/src/aes_asm.asm
        wolfcrypt/src/aes_asm.s
        examples/echoclient/
        examples/echoserver/
        wolfcrypt/user-crypto

    Uncheck "Create top level folder". Click Finish.

3. To include the path to the wolfSSL header files, right click on the project
and go to Properties > Build Properties and select the "Paths" tab. Click "Add"
then "Browse" and select:

        <path_to_wolfssl>/

    Click "OK" then "OK" again.

4. In ```<path_to_wolfssl>/wolfssl/wolfcrypt/settings.h```, uncomment

        #define WOLFSSL_VXWORKS

Note: pthreads defined by default

5. If using the VxWorks simulator add the following to EXTRA\_DEFINE:

        -DVXWORKS_SIM /* only if using the VxWorks simulator */

    This can be done by right clicking on the project in Project Explorer, going to
    Build Properties and selecting the "Variables" tab. Highlight EXTRA\_DEFINE and
    click "Edit". Enter the above define to the end of the line.

6. Copy the certs folder in ```<path_to_wolfssl>/``` to the Wind River Workbench
workspace folder. This is where the simulator looks for the filesystem.

7. Include Entropy:

    - Create a new project, similar to step 1 but choose VxWorks Source Build
    Project as the type of project instead of VxWorks Image Project.
    - In the project directory, double click "Source Build Configuration" and
    under os > core > CORE\_KERNEL Menu > VxWorks Kernel Component
    Configuration find "Inject entropy in interrupts". Double click this.
    - Go back to your VIP project. Right click the project and select "Properties".
    - In "Properties", select "Project References". Check the box next to the
    new project you created. Click "Ok".
    - Rebuild the project.

#### 2 Testing wolfSSL with VxWorks:
##### 2.1 wolfCrypt Test and Benchmark Applications
The wolfCrypt test application will test each of the cryptographic algorithms
and output the status for each as a success or failure. The benchmark application will output the runtime of the cryptographic algorithms in milliseconds.

1. Include the following at the top of usrAppInit.c:

        #include <wolfcrypt/test/test.h>
        #include <wolfssl/ssl.h>
        #include <wolfssl/wolfcrypt/settings.h>
        #include <wolfssl/test.h>
        extern int benchmark_test(void* args);

2. In usrAppInit.c, make a call to the wolfCrypt test and benchmark applications
by adding the following to the usrAppInit() function:

        typedef struct func_args {
	        int    argc;
	        char** argv;
	        int    return_code;
	        tcp_ready* signal;
	        callback_functions *callbacks;
	    } func_args;

	    func_args args;

	    wolfcrypt_test(&args);
	    benchmark_test(&args);

3. Right click on the project and select "Build Project".

4. To run the VxWorks simulator, click the dropdown list next to "VxWorks Simulator" at the top of Workbench and go to "Open Connection Details". Add the correct Kernel Image file. This will be located in ```workspace/<project_name>/default/vxWorks```. Click Apply. Start the simulator by clicking the green, "Connect 'VxWorks Simulator'" button to the right of the "VxWorks Simulator" dropdown list. Verify in the simulator terminal that all wolfCrypt tests pass.

##### 2.2 Example Client
The wolfSSL example client.c file can be found in ```<path_to_wolfssl>/wolfssl/examples/client```.

1. Add the following include to usrAppInit.c:

        #include <examples/client/client.h>

2. In usrAppInit.c, include the func\_args as described in the Test Application
section, and add a call to the client function:

        client_test(&args);

3. The char* host in ```examples/client/client.c``` will need to be changed to the IP address to connect to. For example:

        char* host = "192.168.15.1";

4. Right click on the project and select "Build Project".

5. If using the VxWorks Simulator, localhost will not work. NAT should be selected in the Simulator Connection Advanced setup. To do this, click the dropdown button next to VxWorks Simulator at the top of Workbench and select "Open Connection Details". Make sure the correct kernel image file is selected for you project as stated in section 3.1 step 4. Then click Advanced and select NAT as the Network Config. Click OK and Apply.

6. There is an example server in ```<path_to_wolfssl>``` that can be used for testing the client. wolfSSL will first need to be built. Follow the instructions [here](https://www.wolfssl.com/wolfSSL/Docs-wolfssl-manual-2-building-wolfssl.html) to do so. See the [wolfSSL manual]( https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-3-getting-started.html) for instructions on setting up the example server. From within ```<path_to_wolfssl>/wolfssl```, the following command can be used to run the server on the host machine:

        ./examples/server/server -d -b

7. Start the example client in Workbench by following step 3 in section 3.1.

8. The following output should be expected in the simulator terminal:

        SSL version is TLSv1.2
        SSL cipher suite is TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
        Server response: I hear you fa shizzle!

##### 2.3 Example Server
The example server requires more configuration than the client if using the
VxWorks simulator.

Note: The wolfSSL example server and client cannot run at the same time on the VxWorks simulator. Either remove or comment out the ```client_test(&args);``` line.

1. Add the following include to usrAppInit.c:

        #include </examples/server/server.h>

2. In usrAppInit.c, after the ```func_args args;``` call, add:

        tcp_ready ready;
	    ready.ready = 0;
	    ready.port = 0;
	    args.signal = &ready;

        server_test(&args);

3. Right click on the project and select "Build Project".

4. Start the server and complete the following:
    Go to "Open Connection Details" under VxWorks Simulator which is in the connections
    dropdown. Follow step 4 of section 3.1 to select the correct kernel image for the simulator. In "Open Connection Details" of the simulator, go to "Advanced...". Select simnetd from the Network Config dropdown and enter
    192.168.200.1 as the IP address. To connect to the server running on the VxWorks Simulator, enter these commands
    into the host machine's terminal from any directory (for Ubuntu 14.04):

        sudo openvpn --mktun --dev tap0

    Note: openvpn may need to be installed first.

    In Wind River directory on the host machine:

        sudo vxworks-7/host/x86-linux2/bin/vxsimnetd

    This will start the vxsimnetd application. Leave it running in the background.

5. There is an example client in ```<path_to_wolfssl>/wolfssl/examples``` . Again, wolfSSL will first need to be built. Follow the instructions [here](https://www.wolfssl.com/wolfSSL/Docs-wolfssl-manual-2-building-wolfssl.html) to do so. See the [wolfSSL manual]( https://wolfssl.com/wolfSSL/Docs-wolfssl-manual-3-getting-started.html) for instructions on how to set up the client. From within ```<path_to_wolfssl>/wolfssl``` , the following command can be used to run the client on the host machine:

        ./examples/client/client -h 192.168.200.1

6. The following output should be expected in the simulator terminal:

        SSL version is TLSv1.2
        SSL cipher suite is TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384
        Client message: hello wolfssl!

#### 3 Necessary Files if Using VxWorks Simulator
The following files are required to replicate this build:
* vxsim\_linux\_1\_0\_2\_2 (directory)
* compilers/gnu-4.8.1.5/include/c++/4.8
* compilers/gnu-4.8.1.5/include/c++/4.8/i586-wrs-vxworks
* compilers/gnu-4.8.1.5/lib/gcc/i586-wrs-vxworks/4.8.1/include
* compilers/gnu-4.8.1.5/lib/gcc/i586-wrs-vxworks/4.8.1/include-fixed
* vsb\_vxsim\_linux/share/h
* vsb\_vxsim\_linux/krnl/h/system
* vsb\_vxsim\_linux/krnl/h/public
* vsb\_vxsim\_linux/krnl/configlette
* vsb\_vxsim\_linux/h

Note: This project was tested with a pre-built image in the VxWorks distribution
called vip\_vxsim\_linux\_gnu.

