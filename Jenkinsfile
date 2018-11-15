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
    stage('Build Main Targets') {
      parallel {
        stage('Build iOS') {
          agent {
            node {
              label 'mac'
            }
          }
          steps {
            sh './BuildScripts/ios.sh'
            dir('Build/ios/') {
              stash name: 'ios', includes: 'Binaries/**'
            }
          }
          post {
            cleanup {
              deleteDir()
            }
          }
        }
      }
    }
    stage('Create Build Artifacts') {
      agent {
        node {
          label 'master'
        }
      }
      steps {
        dir('SDK')
        {
          sh 'rm -rf ./Binaries/*'
          unstash 'ios'
          sh 'zip -r -0 ${GIT_COMMIT}.zip ./'
        }
      }
      post {
        success {
          dir('SDK') {
            archiveArtifacts artifacts: "${GIT_COMMIT}.zip", fingerprint: true
          }
        }
      }
    }
  }
}
