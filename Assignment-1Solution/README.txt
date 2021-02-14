
There are two folders - server & client.
Each folder has a separate make files which would compile according to the intructions mentioned in the problem statement.

Lets see how to run the code.

Naviagate to server folder
>make
>./server <port_no>

Naviagate to client folder
>make
>./client <server_IP/HOstName> <port_no> 




Description - Client.

Implemented a list of commands as mentioned in the problem statement-
1. GET - get the file from the server; if the file doesnt exist in server an error message is displayed. Usage  - get <filename>
2. PUT - put the file from the client side to the server side. If the file doesnt exist in server an error message is displayed Usage - put <filename>
3. LS - list all the files in the current server directory. Usage - ls
4. DELETE - delete specified filename; and show if it was successful or not. Usage - delete <filename>
5. EXIT - Terminate the session between the client and the server. Usage - exit


Description - Server.
Server listens to the commands inputed on the client sides and perfoms appropriate actions.
Server respons to the commands such as GET, PUT, DELETE, LS, EXIT. Any additional commands, server gracefully reverts the error message without landing into any issues.
Server is equpped with possible timeouts & retry mechanims and these can be changed accordingly.

Reliable Transer.
I have tested reliable transfer as well with the server details provided in the class. A simple acknowledgement retry is implemented incase of acknowledge is not received by both the parties during a GET & PUT commands.
I have verified the same with more than 100 mb files. I have also seen packet drops inbetween and both the server and client have gracefully retried the dropped packets incase of GET & PUT commands.


Further possibilites.
1)Proper Encription 
2) Simple UI 
