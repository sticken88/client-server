# client-server

This is a project I developed as a practical project for the course of Distributed Programming at Politecnico di Torino, my University.
I've just thought to clean it a little bit, to make the code smarter and more readable. 
I'm planning to replace the character interface used at that time with a more powerful GUI implemented with Qt.

Basically the project is based on a client and a server which can exchange any kind of data between them. I'll try to write down the feature of the program.

### Todo
- creating folders for every object or a folder called 'utils'
- divide the main in 'initializing', 'parsing request(s)' and 'process/serve'
- introducing a qt gui in order to get rid of the character interface
- writing the features of the code

### Done
- create a class to manage the log file(s)
- create a class to manage the socket
- create a class to manage the list of commands
- adding a Makefile to build the project
