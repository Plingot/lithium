#! /bin/bash

if [ $# -eq 0 ]; then
  echo "Start or stop the mysql test server."
  echo
  echo "Usage: " $0 "start|stop"
  exit 1
fi

if [ $1 == "start" ]; then

  docker start lithium-postgres-test
  if [ $? -ne 0 ]; then 
    docker pull postgres
    docker run -p=32768:5432 --name lithium-postgres-test -e POSTGRES_PASSWORD=lithium_test -d postgres
  fi

  docker start lithium-mysql-test
  # echo $?
  if [ $? -ne 0 ]; then 
    docker pull mysql:latest
    docker run -p=14550:3306 --name lithium-mysql-test --health-cmd='mysqladmin ping --silent' -e MYSQL_ROOT_HOST=% -e MYSQL_DATABASE=mysql_test -e MYSQL_ROOT_PASSWORD=lithium_test -d mysql:latest

  while [ $(docker inspect --format "{{json .State.Health.Status }}" lithium-mysql-test) != "\"healthy\"" ]; do 
      printf "."; sleep 1;
    done
  fi 
elif [ $1 == "stop" ]; then

  docker stop lithium-mysql-test
  docker stop lithium-postgres-test

fi
