# This Dockerfile is used to build an image containing basic stuff to be used as a Jenkins slave build node.
FROM ubuntu:16.04
MAINTAINER Oleg Gumbar <brigthside@fonline-status.ru>
LABEL Description=" This image is for building U-Boot inside a container"

# Make sure apt is happy
ENV DEBIAN_FRONTEND=noninteractive

# add i386 architecture

RUN sed -i 's/main$/main universe/' /etc/apt/sources.list
RUN dpkg --add-architecture i386

RUN apt-get -qq update && apt-get -qqy dist-upgrade

# Install a basic SSH server
RUN apt-get install -y openssh-server
RUN mkdir -p /var/run/sshd


# Install JDK 8 (most stable edition) for Jenkins
RUN apt-get install -y --no-install-recommends default-jdk

# Install dependencies

RUN apt-get install -y git openssh-client bash unzip curl build-essential cmake unzip wput ant libx11-dev freeglut3-dev libssl-dev libevent-dev libx11-dev libxi-dev libxmu-dev && \
  apt-get install -y gcc-multilib g++-multilib python2.7 nodejs default-jre git-core wget libsdl2-dev && \
  apt-get autoclean

RUN curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | bash
RUN apt-get -qq update && apt-get install -y git-lfs

# User and permissions for jenkins
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
