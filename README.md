# URL_Filter_Project

## Proxy Server in C

This project is a simple proxy server used for URL filtering and implemented in C. It supports request caching and URL blocking functionality. There are 2 separate files, one for HTTP proxy and other for HTTPS proxy. Following are the steps and guide based on HTTP Proxy, but similar steps shall be followed for HTTPS proxy file as well.

## How to Run

1. Compile the proxy server code using gcc. For example:

```
gcc -o HTTP_Proxy HTTP_Proxy.c
```
2. Run the compiled binary. You can specify the port number for the proxy server to listen on using the -p option. For example, to have the proxy server listen on port 8080:
```
./proxy -p 8080
```
If you don't specify a port number, the program will prompt you to enter one when it starts.

## Blocklist
The proxy server supports blocking specific URLs. The list of blocked URLs is stored in a file named Blocklist.txt in the same directory as the proxy server program.

You can add URLs to the blocklist by entering the a command at the proxy server's command prompt, followed by the URL to block. For example:
```
Enter a command (a - add, r - remove, l - list, q - quit): r
Enter the URL to remove: www.example.com
```
You can view the current blocklist by entering the `l` command at the proxy server's command prompt. For example:
```
Enter a command (a - add, r - remove, l - list, q - quit): l
```
The blocklist is saved to the Blocklist.txt file whenever a URL is added or removed. The proxy server also loads the blocklist from this file when it starts.

## Cache
The proxy server caches responses from the target servers. The cache size is limited to 10 entries. When the cache is full, the least recently used entry is replaced.

## Quitting
You can quit the proxy server by entering the q command at the proxy server's command prompt. For example:
```
Enter a command (a - add, r - remove, l - list, q - quit): q
```
