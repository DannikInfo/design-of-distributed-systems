docker run -d -it -p 8001:8001 --env PORT=8001 --name s1 prs-server
sleep 3 
docker run -d -it -p 8002:8002 --env PORT=8002 --name s2 prs-server 
sleep 3
docker run -d -it -p 8003:8003 --env PORT=8003 --name s3 prs-server 
sleep 3
docker run -d -it -p 8004:8004 --env PORT=8004 --name s4 prs-server 
sleep 3
docker run -d -it -p 8005:8005 --env PORT=8005 --name s5 prs-server 
