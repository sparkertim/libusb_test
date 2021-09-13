# libusb_test
Libusb Test Code

VS Code tasks.json

{

    "version": "2.0.0",
    
    "tasks": [
        {
        
            "label": "Build",
            "type": "shell",
            "options": {
                "cwd": "${workspaceRoot}"
            },
            "command": "g++",
            "args": [
                "-std=c++11",
                "-g",
                "-O0",
                "-fpermissive",
                "hotplugtest.cpp",
                "-lusb-1.0",
                "-lpthread",
                "-o",
                "hotplugtest"
            ],
            
            "problemMatcher": []
        }
    ]
}

