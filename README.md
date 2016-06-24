# particle-aws-iot-listener
C/C++ code for a Particle which receives state info from AWS-IOT via bridge software running on a server.

This code simply blinks the Particle's built-in blue LED based on configuration state updates recieved via AWS-IoT.

It is meant as an example - change the configuration variables and tasks to fit your specific application.

It is designed for a battery operated environment, where the Particle is asleep much of the time to save power. When the Particle wakes up, it retrieves the thingShadow state information and performs one or more tasks if the current state is different than the desired state.
