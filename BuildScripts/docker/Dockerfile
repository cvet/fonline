# This Dockerfile is used to build an image containing basic stuff to be used as a Jenkins slave build node.
FROM ubuntu:18.04
MAINTAINER Oleg Gumbar <brigthside@fonline-status.ru>
LABEL Description="This image is for building FOnline SDK inside a container"

# Make sure apt is happy
ENV DEBIAN_FRONTEND=noninteractive

# Add i386 architecture
#RUN sed -i 's/main$/main universe/' /etc/apt/sources.list
#RUN dpkg --add-architecture i386

# RUN apt-get -qq update && apt-get -qqy dist-upgrade

# Install dependencies
RUN apt-get -qq update && apt-get install -y default-jdk git openssh-client bash unzip curl python-minimal nodejs default-jre git-core wget lftp cmake ant \
  build-essential cmake freeglut3-dev libssl-dev libevent-dev libx11-dev libxi-dev libsdl2-dev

# Install i386 dependencies
#RUN apt-get install -y freeglut3-dev:i386 libssl-dev:i386 libevent-dev:i386 libx11-dev:i386 libxi-dev:i386 libxmu-dev:i386 \
#  libxext-dev:i386 libxrandr-dev:i386 libxcursor-dev:i386 libxi-dev:i386 libxinerama-dev:i386 libxxf86vm-dev:i386 \
#  libxss-dev:i386 libgl1-mesa-dev:i386 libesd0-dev:i386 libdbus-1-dev:i386 libudev-dev:i386 libgles2-mesa-dev:i386 libmirclient-dev:i386 libxkbcommon-dev:i386 libegl1-mesa-dev:i386 libsdl2-dev:i386 \
#  libxkbcommon-dev:i386 libxt-dev:i386 libxv-dev:i386

# Install git-lfs for pulling large binaries
RUN curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | bash
RUN apt-get -qq update && apt-get install -y git-lfs

# Cleaning a bit
RUN apt-get autoclean

# Install jenkins agent
ARG user=jenkins
ARG group=jenkins
ARG uid=10000
ARG gid=10000

ENV HOME /home/${user}
RUN groupadd -g ${gid} ${group}
RUN useradd -c "Jenkins user" -d $HOME -u ${uid} -g ${gid} -m ${user}

ARG VERSION=3.27
ARG AGENT_WORKDIR=/home/${user}/agent

RUN curl --create-dirs -sSLo /usr/share/jenkins/slave.jar https://repo.jenkins-ci.org/public/org/jenkins-ci/main/remoting/${VERSION}/remoting-${VERSION}.jar \
  && chmod 755 /usr/share/jenkins \
  && chmod 644 /usr/share/jenkins/slave.jar

USER ${user}
ENV AGENT_WORKDIR=${AGENT_WORKDIR}
RUN mkdir /home/${user}/.jenkins && mkdir -p ${AGENT_WORKDIR}

VOLUME /home/${user}/.jenkins
VOLUME ${AGENT_WORKDIR}
WORKDIR /home/${user}

COPY jenkins-slave /usr/local/bin/jenkins-slave

ENTRYPOINT ["jenkins-slave"]
