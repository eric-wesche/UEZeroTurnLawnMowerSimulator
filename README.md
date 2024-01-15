# Setup

Add SocketIOClient-Unreal plugin. My fork here: https://github.com/eric-wesche/SocketIOClient-Unreal/tree/fix-build-errors. Then, if you get build errors, checkout my commit: https://github.com/eric-wesche/SocketIOClient-Unreal/commit/16c01f910c0defb88567881262c80b304df1356d.

There is not a convenient way to get my code, because I didn't include every file that ue creates when you create a project. So, one thing you could do is get my code first. Then, build ue5 from source, run it, and then create a project using the c++ vehicle template. This way you have all the ue files that are not in this repo. Then, replace the source files with mine.

I made a minor modification to the Chaos Vehicle Plugin in the engine source code. This was to add left and right throttle for the back wheels. You'll see in my code, I overrode UChaosWheeledVehicleMovementComponent and UChaosWheeledVehicleSimulation so that I could make the car operate like a zero-turn mower. In order to make this work with the existing plugin code, I had to add LeftThrottleInput and RightThrottleInput to ChaosVehicleManagerAsync.h, and add them to the FControlInputs constructor. I currently have issues with my engine git and haven't pushed those changes to my fork of the engine source code.
