# this is the demo file to run all test cases for the HTTP retriever and web server

# test the server alone - running on port 2087
echo "Test 1: Real Web browser accessing my server"
echo "Open a browser and access http://csslab#.uwb.edu:2087/ or http://localhost:2087/ : where the lab# is the number of the computer you are running the server"
echo "The server will sleep for 30 to give time to test within the web browser -- could not think of any other way to test this in a shell script\n"
#start webserver in the background and pause for a moment while it loads
./webserver &
sleep 30
echo ""

# test the retriever alone - running on port 80
echo "Test 2: My retriever accessing a real server -- this index.html becomes the one that is sent by my server in test 3"
./retriever 80 GET www.neverssl.com/
echo ""

# All of the following tests are between my retriever and my server -- both running on port 2087
echo "Notice: All of the following tests are between my retriever and my server -- both running on port 2087"

echo "Test 3: My retriever accessing a file from my server -- ends up being stored as index.html or overwriting the existing index.html"
./retriever 2087 GET 127.0.0.1/index.html
echo ""

echo "Test 4: My retriever sending a request with an unallowed method (POST in this case) to my server"
./retriever 2087 POST 127.0.0.1/index.html
echo ""

echo "Test 5: My retriever requesting a forbidden file from my server"
./retriever 2087 GET 127.0.0.1/forbidden.html
echo ""

echo "Test 6: My retriever requesting a non-existent file from my server"
./retriever 2087 GET 127.0.0.1/IDontExist.html
echo ""

echo "Test 7: My retriever sending a malformed (in this case has no file extension ie: .html) request to my server"
./retriever 2087 GET 127.0.0.1/index
echo ""

# Extra tests just for fun
echo "Notice: The tests below are extra tests just for fun"

echo "Extra Test 1: My retriever requesting an unauthorized file"
./retriever 2087 GET 127.0.0.1/unauthed.html
echo ""

echo "Extra Test 2: My retriever sending a COFFEE request"
./retriever 2087 COFFEE 127.0.0.1/index.html
echo ""

# shut down the webserver so it isn't running when we do this again.
killall webserver
