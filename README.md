# Computer Networks (RCOM) – Labs

## About

The **RCOM** course focuses on understanding, designing, and implementing computer communication systems.
The labs provide hands-on experience with real communication protocols, low-level programming, and practical network configuration in Linux.

---

# Labs Overview

---

# **Lab 1 – Data Link Layer Protocol & File Transfer**

In this lab, a custom **Data Link Layer protocol** is implemented to enable reliable communication between two computers connected through a serial port (RS232).

### **Main Objectives**

* Build a **byte-oriented framed protocol**
* Implement:

  * Frame delimiters (`FLAG`)
  * Address, Control, and BCC fields
  * Byte stuffing / destuffing
* Develop **finite state machines** for:

  * Transmitter (Tx)
  * Receiver (Rx)
* Implement **timeouts** and **retransmissions**
* Transfer a binary file (e.g., `penguin.gif`) using the implemented protocol

### **Components Developed**

* `llopen()`
* `llwrite()`
* `llread()`
* `llclose()`
* Framing, error detection (BCC), and recovery mechanisms

---

# **Lab 2 – Computer Network Configuration**

Lab 2 introduces practical aspects of **network configuration**, **DNS**, **routing**, and the implementation of a minimal **FTP client** using TCP sockets.

The lab is divided into **two parts**, according to the official guide.

---

# **Lab 2 – Part 1: FTP Client Implementation**

This part consists of developing a C application capable of downloading a file from an FTP server using TCP sockets and FTP commands defined in **RFC 959**.

### **Goal**

Creation of an application:

```
./download ftp://[user:password@]host/path/to/file
```

which downloads the specified file and stores it locally.

### **Required Features**

#### URL Parsing (RFC 1738)

Supported format:

```
ftp://[user:password@]host/path
```

If no credentials are provided:

* `user = "anonymous"`
* `password = "anonymous@"`

#### DNS Resolution

Using:

* `gethostbyname()`

#### Control Connection (TCP Port 21)

Establish a TCP connection to the server’s FTP control port.

#### FTP Command Exchange

Send and interpret FTP commands and responses:

Commands:

* `USER`
* `PASS`
* `PASV`
* `RETR`
* `QUIT`

Relevant response codes:

* **220** – Service ready
* **230** – User logged in
* **227** – Entering Passive Mode
* **150 / 125** – Opening data connection
* **226** – Transfer complete

#### Passive Mode Interpretation

The server responds with:

```
227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
```

The data connection endpoint is computed as:

```
IP   = h1.h2.h3.h4
PORT = p1*256 + p2
```

#### Data Connection

A second TCP connection must be opened to the data port returned by PASV.

#### ✔ File Transfer

The file’s binary contents must be received through the data socket and saved to disk.

#### Connection Termination

Both connections (control and data) must be closed correctly following protocol rules.

---

# **Lab 2 – Part 2: Network Experiments**

This part involves performing **six practical experiments** to explore and analyze network behavior in a controlled virtual environment.

---

## **Experiment 1 – Network Interfaces**

Objectives:

* Identify available network interfaces
* Inspect IP addresses, masks, and interface states
* Test reachability using ICMP (`ping`)

Tools:

* `ifconfig`
* `ip addr`
* `ip link`

---

## **Experiment 2 – Routing and Gateway**

Objectives:

* Observe gateway behavior
* Add/remove routes
* Understand the routing table

Tools:

* `route -n`
* `ip route`
* `traceroute`

---

## **Experiment 3 – NAT and Virtual Machine Networking**

Objectives:

* Analyze how NAT enables communication with external networks
* Observe packet forwarding through the virtual machine environment

Tools:

* VirtualBox network configuration
* Linux network utilities

---

## **Experiment 4 – DNS Resolution**

Objectives:

* Perform DNS lookups
* Understand the resolver configuration

Tools:

* `nslookup`
* `dig`
* `/etc/resolv.conf`

---

## **Experiment 5 – TCP Connections**

Objectives:

* List active TCP connections
* Inspect socket states (LISTEN, ESTABLISHED, etc.)

Tools:

* `netstat -an`
* `ss -t -a`

---

## **Experiment 6 – FTP Download Analysis**

Objectives:

* Capture and analyze FTP control and data connections
* Observe FTP commands exchanged during a file transfer
* Analyze binary transfer behavior

Tools:

* The implemented FTP client
* Wireshark

---

# Technologies Used

* C programming (POSIX sockets)
* Serial communication (Lab 1)
* TCP/IP networking
* DNS, routing, NAT
* Linux networking tools
* Wireshark

---

# Authors

* *João Ferreira*
* *Gonçalo Santos*

---

📘 *This repository is part of the Computer Networks (RCOM) course at FEUP.*



