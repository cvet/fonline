pipeline {
  environment {
    FO_BUILD_DEST = 'Build'
    FO_SOURCE = '.'
    FO_INSTALL_PACKAGES = 0
  }
  options {
      disableConcurrentBuilds()
  }
  agent none
  stages {
    stage('Build Main targets') {
      parallel {
        stage('Build Web') {
          agent {
            kubernetes {
              label 'linux'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            container('jnlp') {
              sh './BuildScripts/web.sh'
              dir('Build/web/'){
                stash name: 'web', includes: 'Binaries/**'
              }
            }
          }
        }
        stage('Build Windows') {
          agent {
            node {
              label 'win'
            }
          }
          steps {
            bat 'BuildScripts\\windows.bat'
            dir('Build/windows/'){
              stash name: 'windows', includes: 'Binaries/**'
            }
          }
          post {
            cleanup{
              deleteDir()
            }
          }
        }
      }
    }

    stage('Create build artifact') {
      agent {
        node {
          label 'master'
        }
      }
      steps {
        dir('SDK')
        {
          sh 'rm -rf ./Binaries/*'
          unstash 'linux'
          unstash 'android'
          unstash 'windows'
          unstash 'mac'
          unstash 'web'
          sh 'zip -r -0 ${GIT_COMMIT}.zip ./'
        }
      }
      post {
        success{
          dir('SDK'){
            archiveArtifacts artifacts: "${GIT_COMMIT}.zip", fingerprint: true
          }
        }
      }
    }
  }
} 