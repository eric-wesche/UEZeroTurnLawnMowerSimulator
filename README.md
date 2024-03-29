# Low-quality demo of scene

![](https://github.com/eric-wesche/UEZeroTurnLawnMowerSimulator/blob/working/1/scene_demo.gif)


# Setup

Add SocketIOClient-Unreal plugin. My fork here: https://github.com/eric-wesche/SocketIOClient-Unreal/tree/fix-build-errors. Then, if you get build errors, checkout my commit: https://github.com/eric-wesche/SocketIOClient-Unreal/commit/16c01f910c0defb88567881262c80b304df1356d.

There is not a convenient way to get my code, because I didn't include every file that ue creates when you create a project. So, one thing you could do is get my code first. Then, build ue5 from source, run it, and then create a project using the c++ vehicle template. This way you have all the ue files that are not in this repo. Then, replace the source files with mine.

I made a minor modification to the Chaos Vehicle Plugin in the engine source code. This was to add left and right throttle for the back wheels. You'll see in my code, I overrode UChaosWheeledVehicleMovementComponent and UChaosWheeledVehicleSimulation so that I could make the car operate like a zero-turn mower. In order to make this work with the existing plugin code, I had to add LeftThrottleInput and RightThrottleInput to ChaosVehicleManagerAsync.h, and add them to the FControlInputs constructor. I currently have issues with my engine git and haven't pushed those changes to my fork of the engine source code.

The vehicle pawn is AMower3OffroadCar. The inputs are specified in the parent class. There are four inputs, forward and back for each throttle. Left throttle is keys q,z; right throttle is e,c. See Content/Inputs/IMC_Default. 

For the level that I use, the grass, etc, I will find a way to allow you to create what you need. I will add the assets that I use if possible, however the level is too large so I will need to use git lfs. In the meantime, you could use the code here now and create your own level, or uses pieces of my code for your own work. You can see how I removed the grass to simulate cutting it in ReplaceOrRemoveGrass function of Mower3OffroadCar.cpp.

For getting frame data, processing it (eg per pixel segmentation of objects) and sending to server see CaptureManager.h. My idea is to use this data in the python server to run an ml algorithm, and then as you can see I have the socket connect setup such that the python server sends values for left and right throttle to the car. You can see the car class and uncomment relevant code in order to see the server control the car instead of you. With this setup, you can either show the algo expert samples, or you can just have for example have an rl algorithm where the ai explores itself. You can also take control, thus for example getting it unstuck.

# Sources
I got the initial capture manager code from https://github.com/TimmHess/UnrealImageCapture/tree/master, however mine is heavily modified.

# Contribution
Feel free to make prs. There are many things to do for this project.

I have not added per pixel depth (or distance to, say, different areas of the vehicle), however this could help: https://github.com/unrealgt/unrealgt/blob/master/Source/UnrealGT/Private/Generators/Image/GTDepthImageGeneratorComponent.cpp. 

I have not added machine learning algorithms to control the mower. You could for example add a reinforcement learning algorithm.

I have a zero-turn mower and I intend to make it autonomous. If anyone wants to work on this or has done so, I am interested.

There are likely many ways to optimize the code.
