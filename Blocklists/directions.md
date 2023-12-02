# URL Blocklist File
This file contains a list of URLs to be blocked by the HTTP/HTTPS proxy server.

Instructions for Adding URLs:
Each URL must be listed on a separate line.
URLs should be written in the following format:
Correct Format: www.example.com
Incorrect Format: "www.example.com" (Avoid using quotes)
Example:

## URLs to Block

www.youtube.com
www.facebook.com
www.example.com

*Explanation:*

The .txt file serves as a blocklist for the proxy server.
Each URL to be blocked is listed on a separate line without any additional characters like quotes.
Proper formatting (www.example.com) ensures that the proxy server accurately recognizes and blocks the specified URLs.
To add more URLs to the blocklist, simply follow the instructions outlined above. Remember to save the file after making any changes.

This file is directly read by the C code implementation of the proxy server, ensuring that the listed URLs are effectively blocked.
