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
        stage('Build Android') {
          agent {
            kubernetes {
              label 'linux'
              yamlFile 'BuildScripts/build-pod.yaml'
            }
          }
          steps {
            sh './BuildScripts/android.sh'
            dir('Build/android/') {
              stash name: 'android', includes: 'Binaries/**'
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
          unstash 'android'
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
