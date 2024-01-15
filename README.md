# Setup

Add SocketIOClient-Unreal plugin. My fork here: https://github.com/eric-wesche/SocketIOClient-Unreal/tree/fix-build-errors. Then, if you get build errors, checkout my commit: https://github.com/eric-wesche/SocketIOClient-Unreal/commit/16c01f910c0defb88567881262c80b304df1356d.

There is not a convenient way to get my code, because I didn't include every file that ue creates when you create a project. So, one thing you could do is get my code first. Then, build ue5 from source, run it, and then create a project using the c++ vehicle template. This way you have all the ue files that are not in this repo. Then, replace the source files with mine.


